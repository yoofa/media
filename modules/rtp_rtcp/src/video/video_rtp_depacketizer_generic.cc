/*
 * video_rtp_depacketizer_generic.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_generic.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <utility>

#include "base/copy_on_write_buffer.h"
#include "base/logging.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace {
constexpr uint8_t kKeyFrameBit = 0b0000'0001;
constexpr uint8_t kFirstPacketBit = 0b0000'0010;
// If this bit is set, there will be an extended header contained in this
// packet. This was added later so old clients will not send this.
constexpr uint8_t kExtendedHeaderBit = 0b0000'0100;

constexpr size_t kGenericHeaderLength = 1;
constexpr size_t kExtendedHeaderLength = 2;
}  // namespace

std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerGeneric::Parse(base::CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.empty()) {
    AVE_LOG(LS_WARNING) << "Empty payload.";
    return std::nullopt;
  }
  std::optional<ParsedRtpPayload> parsed(std::in_place);
  const uint8_t* payload_data = rtp_payload.cdata();

  uint8_t generic_header = payload_data[0];
  size_t offset = kGenericHeaderLength;

  parsed->video_header.frame_type = (generic_header & kKeyFrameBit)
                                        ? VideoFrameType::kVideoFrameKey
                                        : VideoFrameType::kVideoFrameDelta;
  parsed->video_header.is_first_packet_in_frame =
      (generic_header & kFirstPacketBit) != 0;
  parsed->video_header.codec = VideoCodecType::kVideoCodecGeneric;
  parsed->video_header.width = 0;
  parsed->video_header.height = 0;

  if (generic_header & kExtendedHeaderBit) {
    if (rtp_payload.size() < offset + kExtendedHeaderLength) {
      AVE_LOG(LS_WARNING) << "Too short payload for generic header.";
      return std::nullopt;
    }
    // Extended header contains picture_id, skip it for now since we don't have
    // RTPVideoHeaderLegacyGeneric support in our simplified header.
    offset += kExtendedHeaderLength;
  }

  parsed->video_payload =
      rtp_payload.Slice(offset, rtp_payload.size() - offset);
  return parsed;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
