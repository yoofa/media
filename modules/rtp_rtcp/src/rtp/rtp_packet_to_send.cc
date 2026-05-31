/*
 * rtp_packet_to_send.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include <span>

#include <cstdint>

#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extensions.h"
#include "media/modules/rtp_rtcp/src/util/byte_io.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

RtpPacketToSend::RtpPacketToSend(const ExtensionManager* extensions)
    : RtpPacket(extensions) {}

RtpPacketToSend::RtpPacketToSend(const ExtensionManager* extensions,
                                 size_t capacity)
    : RtpPacket(extensions, capacity) {}

RtpPacketToSend::RtpPacketToSend(const RtpPacketToSend& packet) = default;
RtpPacketToSend::RtpPacketToSend(RtpPacketToSend&& packet) = default;

RtpPacketToSend& RtpPacketToSend::operator=(const RtpPacketToSend& packet) =
    default;
RtpPacketToSend& RtpPacketToSend::operator=(RtpPacketToSend&& packet) = default;

RtpPacketToSend::~RtpPacketToSend() = default;

void RtpPacketToSend::set_packet_type(RtpPacketMediaType type) {
  if (packet_type_ == RtpPacketMediaType::kAudio) {
    original_packet_type_ = OriginalType::kAudio;
  } else if (packet_type_ == RtpPacketMediaType::kVideo) {
    original_packet_type_ = OriginalType::kVideo;
  }
  packet_type_ = type;
}

void RtpPacketToSend::set_packetization_finish_time(base::Timestamp time) {
  // Calculate delta from capture time in milliseconds.
  int64_t delta_ms = (time - capture_time_).ms();
  // Clamp to uint16_t range.
  uint16_t delta_ms_u16 = static_cast<uint16_t>(
      std::min<int64_t>(std::max<int64_t>(delta_ms, 0), 0xFFFF));

  // Write the delta to the VideoTimingExtension at the packetization finish
  // offset.
  uint8_t buffer[2];
  ByteWriter<uint16_t>::WriteBigEndian(buffer, delta_ms_u16);
  UpdateExtensionData(RTPExtensionType::kRtpExtensionVideoTiming,
                      VideoTimingExtension::kPacketizationFinishDeltaOffset,
                      std::span<const uint8_t>(buffer, sizeof(buffer)));
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
