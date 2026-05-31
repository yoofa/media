/*
 * rtp_packet_counter.cc - RtpPacketCounter implementation
 *
 * Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the root of the source tree. An additional
 * intellectual property rights grant can be found in the file PATENTS.
 */

#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

void RtpPacketCounter::AddPacket(const RtpPacket& packet) {
  ++packets;
  header_bytes += packet.headers_size();
  padding_bytes += packet.padding_size();
  payload_bytes += packet.payload_size();
}

void RtpPacketCounter::AddPacket(const RtpPacketToSend& packet_to_send) {
  AddPacket(static_cast<const RtpPacket&>(packet_to_send));
  if (packet_to_send.time_in_send_queue().has_value()) {
    total_packet_delay += *packet_to_send.time_in_send_queue();
  }
}

void RtpPacketCounter::AddPacket(const RtpPacketReceived& packet) {
  AddPacket(static_cast<const RtpPacket&>(packet));
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
