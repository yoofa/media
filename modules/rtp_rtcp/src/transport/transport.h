/*
 * transport.h - Transport interface for RTP/RTCP packets
 *
 * Copyright (c) 2025 The AveStart project authors. All Rights Reserved.
 *
 * Ported from WebRTC project.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_TRANSPORT_TRANSPORT_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_TRANSPORT_TRANSPORT_H_

#include <cstdint>

#include <span>

namespace ave {
namespace media {
namespace rtp_rtcp {

// Packet options for sending RTP/RTCP packets.
struct PacketOptions {
  PacketOptions() = default;
  PacketOptions(const PacketOptions&) = default;
  ~PacketOptions() = default;

  // Negative ids are invalid and should be interpreted
  // as packet_id not being set.
  int64_t packet_id = -1;
  // Whether this is an audio or video packet, excluding retransmissions.
  bool is_media = true;
  bool included_in_feedback = false;
  bool included_in_allocation = false;
  bool send_as_ect1 = false;
  // Whether this packet can be part of a packet batch at lower levels.
  bool batchable = false;
  // Whether this packet is the last of a batch.
  bool last_packet_in_batch = false;
};

// Transport interface for sending RTP and RTCP packets.
class Transport {
 public:
  virtual bool SendRtp(std::span<const uint8_t> packet,
                       const PacketOptions& options) = 0;
  virtual bool SendRtcp(std::span<const uint8_t> packet) = 0;

 protected:
  virtual ~Transport() = default;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_TRANSPORT_TRANSPORT_H_
