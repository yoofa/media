/*
 * rtp_packet_send_info.cc
 * Copyright (C) 2024 WebRTC Project Authors. All Rights Reserved.
 *
 * Ported to AVE project.
 */

#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

#include <cstdint>
#include <optional>

#include "base/checks.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extensions.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/transport/network_types.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

RtpPacketSendInfo RtpPacketSendInfo::From(const RtpPacketToSend& packet,
                                          const PacedPacketInfo& pacing_info) {
  RtpPacketSendInfo packet_info;
  if (packet.transport_sequence_number()) {
    packet_info.transport_sequence_number =
        *packet.transport_sequence_number() & 0xFFFF;
  } else {
    std::optional<uint16_t> packet_id =
        packet.GetExtension<TransportSequenceNumber>();
    if (packet_id) {
      packet_info.transport_sequence_number = *packet_id;
    }
  }

  packet_info.rtp_timestamp = packet.Timestamp();
  packet_info.length = packet.size();
  // Copy pacing info fields
  packet_info.pacing_info.probe_cluster_id = pacing_info.probe_cluster_id;
  packet_info.pacing_info.probe_cluster_min_probes =
      pacing_info.probe_cluster_min_probes;
  packet_info.pacing_info.probe_cluster_min_bytes =
      pacing_info.probe_cluster_min_bytes;
  packet_info.pacing_info.probe_cluster_bytes_sent =
      pacing_info.probe_cluster_bytes_sent;
  packet_info.packet_type = packet.packet_type();

  switch (*packet_info.packet_type) {
    case RtpPacketMediaType::kAudio:
    case RtpPacketMediaType::kVideo:
      packet_info.media_ssrc = packet.Ssrc();
      packet_info.rtp_sequence_number = packet.SequenceNumber();
      break;
    case RtpPacketMediaType::kRetransmission:
      AVE_DCHECK(packet.original_ssrc() &&
                 packet.retransmitted_sequence_number());
      // For retransmissions, we're want to remove the original media packet
      // if the retransmit arrives - so populate that in the packet info.
      packet_info.media_ssrc = packet.original_ssrc().value_or(0);
      packet_info.rtp_sequence_number =
          packet.retransmitted_sequence_number().value_or(0);
      break;
    case RtpPacketMediaType::kPadding:
    case RtpPacketMediaType::kForwardErrorCorrection:
      // We're not interested in feedback about these packets being received
      // or lost.
      break;
  }
  return packet_info;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
