/*
 * create_video_rtp_depacketizer.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/video/create_video_rtp_depacketizer.h"

#include <memory>

#include "base/checks.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_av1.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_generic.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_h264.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_h265.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_vp8.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_vp9.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

std::unique_ptr<VideoRtpDepacketizer> CreateVideoRtpDepacketizer(
    VideoCodecType codec) {
  switch (codec) {
    case VideoCodecType::kVideoCodecH264:
      return std::make_unique<VideoRtpDepacketizerH264>();
    case VideoCodecType::kVideoCodecVP8:
      return std::make_unique<VideoRtpDepacketizerVp8>();
    case VideoCodecType::kVideoCodecVP9:
      return std::make_unique<VideoRtpDepacketizerVp9>();
    case VideoCodecType::kVideoCodecAV1:
      return std::make_unique<VideoRtpDepacketizerAv1>();
    case VideoCodecType::kVideoCodecH265:
      return std::make_unique<VideoRtpDepacketizerH265>();
    case VideoCodecType::kVideoCodecGeneric:
      return std::make_unique<VideoRtpDepacketizerGeneric>();
  }
  AVE_NOTREACHED();
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
