/*
 * rtp_video_layers_allocation_extension.h - RTP video layers allocation
 * extension
 *
 * Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 * Copyright (c) 2024 The aspect Video Engine project authors.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYERS_ALLOCATION_EXTENSION_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYERS_ALLOCATION_EXTENSION_H_

#include <string_view>

#include <span>
#include "media/foundation/video_layers_allocation.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpVideoLayersAllocationExtension {
 public:
  using value_type = VideoLayersAllocation;
  static constexpr RTPExtensionType kId = kRtpExtensionVideoLayersAllocation;
  static constexpr std::string_view Uri() {
    return RtpExtension::kVideoLayersAllocationUri;
  }

  static bool Parse(std::span<const uint8_t> data,
                    VideoLayersAllocation* allocation);
  static size_t ValueSize(const VideoLayersAllocation& allocation);
  static bool Write(std::span<uint8_t> data,
                    const VideoLayersAllocation& allocation);
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYERS_ALLOCATION_EXTENSION_H_
