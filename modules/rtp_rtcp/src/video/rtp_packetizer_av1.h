/*
 * rtp_packetizer_av1.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_AV1_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_AV1_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include <span>
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpPacketizerAv1 : public RtpPacketizer {
 public:
  RtpPacketizerAv1(std::span<const uint8_t> payload,
                   PayloadSizeLimits limits,
                   VideoFrameType frame_type,
                   bool is_last_frame_in_picture,
                   bool even_distribution = true);
  ~RtpPacketizerAv1() override = default;

  size_t NumPackets() const override { return packets_.size() - packet_index_; }
  bool NextPacket(RtpPacketToSend* packet) override;

 private:
  struct Obu {
    uint8_t header;
    uint8_t extension_header;  // undefined if (header & kXbit) == 0
    std::span<const uint8_t> payload;
    int32_t size;  // size of the header and payload combined.
  };
  struct Packet {
    explicit Packet(int32_t first_obu_index) : first_obu(first_obu_index) {}
    // Indexes into obus_ vector of the first and last obus that should put into
    // the packet.
    int32_t first_obu;
    int32_t num_obu_elements = 0;
    int32_t first_obu_offset = 0;
    int32_t last_obu_size;
    // Total size consumed by the packet.
    int32_t packet_size = 0;
  };

  // Parses the payload into serie of OBUs.
  static std::vector<Obu> ParseObus(std::span<const uint8_t> payload);
  // Returns the number of additional bytes needed to store the previous OBU
  // element if an additonal OBU element is added to the packet.
  static int32_t AdditionalBytesForPreviousObuElement(const Packet& packet);
  // Packetize and try to distribute the payload evenly across packets.
  static std::vector<Packet> PacketizeAboutEqually(const std::vector<Obu>& obus,
                                                   PayloadSizeLimits limits);
  static std::vector<Packet> Packetize(const std::vector<Obu>& obus,
                                       PayloadSizeLimits limits);

  uint8_t AggregationHeader() const;

  const VideoFrameType frame_type_;
  const std::vector<Obu> obus_;
  const std::vector<Packet> packets_;
  const bool is_last_frame_in_picture_;
  size_t packet_index_ = 0;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKETIZER_AV1_H_
