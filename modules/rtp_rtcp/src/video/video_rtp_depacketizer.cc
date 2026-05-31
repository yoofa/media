/*
 * video_rtp_depacketizer.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"
#include <span>

namespace ave {
namespace media {
namespace rtp_rtcp {

std::shared_ptr<EncodedImageBuffer> VideoRtpDepacketizer::AssembleFrame(
    std::span<const std::span<const uint8_t>> /*rtp_payloads*/) {
  return nullptr;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
