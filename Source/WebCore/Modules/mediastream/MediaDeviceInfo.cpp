/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "MediaDeviceInfo.h"

#if ENABLE(MEDIA_STREAM)

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaDeviceInfo);

Ref<MediaDeviceInfo> MediaDeviceInfo::create(const String& label, const String& deviceId, const String& groupId, Kind kind)
{
    return adoptRef(*new MediaDeviceInfo(label, deviceId, groupId, kind));
}

MediaDeviceInfo::MediaDeviceInfo(const String& label, const String& deviceId, const String& groupId, Kind kind)
    : m_label(label)
    , m_deviceId(deviceId)
    , m_groupId(groupId)
    , m_kind(kind)
{
}

MediaDeviceInfo::Kind toMediaDeviceInfoKind(CaptureDevice::DeviceType type)
{
    switch (type) {
    case CaptureDevice::DeviceType::Microphone:
        return MediaDeviceInfo::Kind::Audioinput;
    case CaptureDevice::DeviceType::Speaker:
        return MediaDeviceInfo::Kind::Audiooutput;
    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        return MediaDeviceInfo::Kind::Videoinput;
    case CaptureDevice::DeviceType::SystemAudio:
    case CaptureDevice::DeviceType::Unknown:
        break;
    }
    ASSERT_NOT_REACHED();
    return MediaDeviceInfo::Kind::Audioinput;
}

} // namespace WebCore

#endif
