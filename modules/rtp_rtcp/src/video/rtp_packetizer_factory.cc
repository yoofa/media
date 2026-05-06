/*
 * rtp_packetizer_factory.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/video/rtp_format.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include <span>
#include "media/foundation/h264/h264_globals.h"
#include "media/foundation/video_codec_type.h"
#include "media/foundation/video_frame_type.h"
#include "media/foundation/vp8/vp8_globals.h"
#include "media/foundation/vp9/vp9_globals.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format_h264.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format_video_generic.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format_vp8.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format_vp9.h"
#include "media/modules/rtp_rtcp/src/video/rtp_packetizer_av1.h"
#include "media/modules/rtp_rtcp/src/video/rtp_packetizer_h265.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

std::unique_ptr<RtpPacketizer> RtpPacketizer::Create(
    std::optional<VideoCodecType> type,
    std::span<const uint8_t> payload,
    PayloadSizeLimits limits,
    const RTPVideoHeader& rtp_video_header,
    bool enable_av1_even_split) {
  if (!type.has_value()) {
    // Raw payload without codec-specific packetization.
    return std::make_unique<RtpPacketizerGeneric>(payload, limits);
  }

  switch (*type) {
    case VideoCodecType::kVideoCodecH264: {
      const auto* h264_header =
          std::get_if<RTPVideoHeaderH264>(&rtp_video_header.video_type_header);
      H264PacketizationMode packetization_mode =
          h264_header ? h264_header->packetization_mode
                      : H264PacketizationMode::NonInterleaved;
      return std::make_unique<RtpPacketizerH264>(payload, limits,
                                                 packetization_mode);
    }
    case VideoCodecType::kVideoCodecVP8: {
      const auto* vp8_header =
          std::get_if<RTPVideoHeaderVP8>(&rtp_video_header.video_type_header);
      RTPVideoHeaderVP8 header_info{};
      if (vp8_header) {
        header_info = *vp8_header;
      }
      return std::make_unique<RtpPacketizerVp8>(payload, limits, header_info);
    }
    case VideoCodecType::kVideoCodecVP9: {
      const auto* vp9_header =
          std::get_if<RTPVideoHeaderVP9>(&rtp_video_header.video_type_header);
      RTPVideoHeaderVP9 header_info{};
      if (vp9_header) {
        header_info = *vp9_header;
      }
      return std::make_unique<RtpPacketizerVp9>(payload, limits, header_info);
    }
    case VideoCodecType::kVideoCodecAV1:
      return std::make_unique<RtpPacketizerAv1>(
          payload, limits, rtp_video_header.frame_type,
          rtp_video_header.is_last_frame_in_picture, enable_av1_even_split);
    case VideoCodecType::kVideoCodecH265:
      return std::make_unique<RtpPacketizerH265>(payload, limits);
    case VideoCodecType::kVideoCodecGeneric:
      return std::make_unique<RtpPacketizerGeneric>(payload, limits,
                                                    rtp_video_header);
    default:
      // Fallback to generic packetizer.
      return std::make_unique<RtpPacketizerGeneric>(payload, limits,
                                                    rtp_video_header);
  }
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
