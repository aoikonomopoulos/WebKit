/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WPEDisplayWayland.h"
#include <wayland-client.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/TZoneMalloc.h>

namespace WPE {

class WaylandCursorTheme;

class WaylandCursor {
    WTF_MAKE_TZONE_ALLOCATED(WaylandCursor);
public:
    explicit WaylandCursor(WPEDisplayWayland*);
    ~WaylandCursor();

    void setFromName(const char*, double);
    void setFromBuffer(struct wl_buffer*, uint32_t width, uint32_t height, uint32_t hotspotX, uint32_t hotspotY);
    void update() const;

private:
    WPEDisplayWayland* m_display { nullptr };
    struct wl_surface* m_surface { nullptr };
    std::unique_ptr<WaylandCursorTheme> m_theme;
    GUniquePtr<char> m_name;
    struct {
        int32_t x { 0 };
        int32_t y { 0 };
    } m_hotspot;
    mutable bool m_cursorChanged { false };
};

} // namespace WPE
