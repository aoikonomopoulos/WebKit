/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoordinatedGraphicsLayer.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "DisplayListDrawingContext.h"
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "NicosiaBuffer.h"
#include "PlatformDisplay.h"
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/gl/GrGLInterface.h>
#include <skia/gpu/gl/GrGLTypes.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>
#include <wtf/Vector.h>
#include <wtf/WorkerPool.h>

namespace WebCore {

Ref<Nicosia::Buffer> CoordinatedGraphicsLayer::paintTile(const IntRect& tileRect, const IntRect& mappedTileRect, float contentsScale)
{
    auto paintIntoGraphicsContext = [&](GraphicsContext& context) {
        IntRect initialClip(IntPoint::zero(), tileRect.size());
        context.clip(initialClip);

        if (!contentsOpaque()) {
            context.setCompositeOperation(CompositeOperator::Copy);
            context.fillRect(initialClip, Color::transparentBlack);
            context.setCompositeOperation(CompositeOperator::SourceOver);
        }

        context.translate(-tileRect.x(), -tileRect.y());
        context.scale({ contentsScale, contentsScale });
        paintGraphicsLayerContents(context, mappedTileRect);
    };

    auto paintBuffer = [&](Nicosia::Buffer& buffer) {
        buffer.beginPainting();

        if (auto* canvas = buffer.canvas()) {
            canvas->save();
            canvas->clear(SkColors::kTransparent);

            GraphicsContextSkia context(*canvas, buffer.isBackedByOpenGL() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
            paintIntoGraphicsContext(context);

            canvas->restore();
        }

        buffer.completePainting();
    };

    // Skia/GPU - accelerated rendering.
    auto* skiaGLContext = PlatformDisplay::sharedDisplay().skiaGLContext();
    if (auto* acceleratedBitmapTexturePool = m_coordinator->skiaAcceleratedBitmapTexturePool(); acceleratedBitmapTexturePool && skiaGLContext->makeContextCurrent()) {
        OptionSet<BitmapTexture::Flags> textureFlags;
        if (!contentsOpaque())
            textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

        auto buffer = Nicosia::AcceleratedBuffer::create(acceleratedBitmapTexturePool->acquireTexture(tileRect.size(), textureFlags));
        WTFBeginSignpost(this, PaintTile, "Skia accelerated, dirty region %ix%i+%i+%i", tileRect.x(), tileRect.y(), tileRect.width(), tileRect.height());
        paintBuffer(buffer.get());
        WTFEndSignpost(this, PaintTile);

        return buffer;
    }

    // Skia/CPU - unaccelerated rendering.
    auto buffer = Nicosia::UnacceleratedBuffer::create(tileRect.size(), contentsOpaque() ? Nicosia::Buffer::NoFlags : Nicosia::Buffer::SupportsAlpha);

    // Non-blocking, multi-threaded variant.
    if (auto* workerPool = m_coordinator->skiaUnacceleratedThreadedRenderingPool()) {
        WTFBeginSignpost(this, RecordTile);

        // Threaded rendering: record display lists, and asynchronously replay them using dedicated worker threads.
        buffer->beginPainting();

        auto displayList = makeUnique<DisplayList::DisplayList>();
        DisplayList::RecorderImpl recordingContext(*displayList, GraphicsContextState(), FloatRect({ }, tileRect.size()), AffineTransform());
        paintIntoGraphicsContext(recordingContext);

        workerPool->postTask([buffer = Ref { buffer }, displayList = WTFMove(displayList), tileRect]() mutable {
            if (auto* canvas = buffer->canvas()) {
                canvas->save();
                canvas->clear(SkColors::kTransparent);

                static thread_local RefPtr<ControlFactory> s_controlFactory;
                if (!s_controlFactory)
                    s_controlFactory = ControlFactory::create();

                WTFBeginSignpost(canvas, PaintTile, "Skia unaccelerated multithread, dirty region %ix%i+%i+%i", tileRect.x(), tileRect.y(), tileRect.width(), tileRect.height());

                GraphicsContextSkia context(*canvas, RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
                context.drawDisplayListItems(displayList->items(), displayList->resourceHeap(), *s_controlFactory, FloatPoint::zero());

                WTFEndSignpost(canvas, PaintTile);
                canvas->restore();
            }
            buffer->completePainting();

            // Destruct display list on main thread.
            ensureOnMainThread([displayList = WTFMove(displayList)]() mutable {
                displayList = nullptr;
            });
        });

        WTFEndSignpost(this, RecordTile);

        return buffer;
    }

    // Blocking, single-thread variant.
    WTFBeginSignpost(this, PaintTile, "Skia unaccelerated, dirty region %ix%i+%i+%i", tileRect.x(), tileRect.y(), tileRect.width(), tileRect.height());
    paintBuffer(buffer.get());
    WTFEndSignpost(this, PaintTile);

    return buffer;
}

Ref<Nicosia::Buffer> CoordinatedGraphicsLayer::paintImage(Image& image)
{
    // FIXME: can we just get the image texture if accelerated or upload the pixels if not acclerated instead of painting?.
    // Always render unaccelerated here for now.
    auto buffer = Nicosia::UnacceleratedBuffer::create(IntSize(image.size()), !image.currentFrameKnownToBeOpaque() ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
    buffer->beginPainting();
    if (auto* canvas = buffer->canvas()) {
        canvas->save();
        canvas->clear(SkColors::kTransparent);

        GraphicsContextSkia context(*canvas, RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
        IntRect rect { IntPoint::zero(), IntSize { image.size() } };
        context.drawImage(image, rect, rect, ImagePaintingOptions(CompositeOperator::Copy));

        canvas->restore();
    }
    buffer->completePainting();
    return buffer;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
