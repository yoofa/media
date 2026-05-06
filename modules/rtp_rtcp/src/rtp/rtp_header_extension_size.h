/*
 * rtp_header_extension_size.h
 * Ported from WebRTC (modules/rtp_rtcp/source/rtp_header_extension_size.h)
 *
 * Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the root of the source tree. An additional
 * intellectual property rights grant can be found in the file PATENTS.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_SIZE_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_SIZE_H_

#include <span>
#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

struct RtpExtensionSize {
  RTPExtensionType type;
  int32_t value_size;
};

// Calculates rtp header extension size in bytes assuming packet contain
// all `extensions` with provided `value_size`.
// Counts only extensions present among `registered_extensions`.
int32_t RtpHeaderExtensionSize(
    std::span<const RtpExtensionSize> extensions,
    const RtpHeaderExtensionMap& registered_extensions);

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_SIZE_H_
