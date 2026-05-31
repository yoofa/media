/*
 * rtp_packetizer_h265.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_H265_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_H265_H_

#include <deque>
#include <queue>
#include <string>

#include <span>
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpPacketizerH265 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded H.265 frame.
  // For H265 we only support tx-mode SRST.
  RtpPacketizerH265(std::span<const uint8_t> payload, PayloadSizeLimits limits);

  RtpPacketizerH265(const RtpPacketizerH265&) = delete;
  RtpPacketizerH265& operator=(const RtpPacketizerH265&) = delete;

  ~RtpPacketizerH265() override;

  size_t NumPackets() const override;

  // Get the next payload with H.265 payload header.
  // Write payload and set marker bit of the `packet`.
  // Returns true on success or false if there was no payload to packetize.
  bool NextPacket(RtpPacketToSend* rtp_packet) override;

 private:
  struct PacketUnit {
    std::span<const uint8_t> source_fragment;
    bool first_fragment = false;
    bool last_fragment = false;
    bool aggregated = false;
    uint16_t header = 0;
  };
  std::deque<std::span<const uint8_t>> input_fragments_;
  std::queue<PacketUnit> packets_;

  bool GeneratePackets();
  bool PacketizeFu(size_t fragment_index);
  int PacketizeAp(size_t fragment_index);

  void NextAggregatePacket(RtpPacketToSend* rtp_packet);
  void NextFragmentPacket(RtpPacketToSend* rtp_packet);

  const PayloadSizeLimits limits_;
  size_t num_packets_left_ = 0;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_H265_H_
