/*
 * flexfec_receiver.h - FlexFEC receiver
 *
 * Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-video-engine by aspect.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_

#include <stdint.h>

#include <memory>

#include "base/clock.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/recovered_packet_receiver.h"
#include "media/modules/rtp_rtcp/src/fec/forward_error_correction.h"
#include "media/modules/rtp_rtcp/src/fec/ulpfec_receiver.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class FlexfecReceiver {
 public:
  FlexfecReceiver(uint32_t ssrc,
                  uint32_t protected_media_ssrc,
                  RecoveredPacketReceiver* recovered_packet_receiver);
  FlexfecReceiver(base::Clock* clock,
                  uint32_t ssrc,
                  uint32_t protected_media_ssrc,
                  RecoveredPacketReceiver* recovered_packet_receiver);
  ~FlexfecReceiver();

  // Inserts a received packet (can be either media or FlexFEC) into the
  // internal buffer, and sends the received packets to the erasure code.
  // All newly recovered packets are sent back through the callback.
  void OnRtpPacket(const RtpPacketReceived& packet);

  // Returns a counter describing the added and recovered packets.
  FecPacketCounter GetPacketCounter() const;

  // Protected to aid testing.
 protected:
  std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> AddReceivedPacket(
      const RtpPacketReceived& packet);
  void ProcessReceivedPacket(
      const ForwardErrorCorrection::ReceivedPacket& received_packet);

 private:
  // Config.
  const uint32_t ssrc_;
  const uint32_t protected_media_ssrc_;

  // Erasure code interfacing and callback.
  std::unique_ptr<ForwardErrorCorrection> erasure_code_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packets_;
  RecoveredPacketReceiver* const recovered_packet_receiver_;

  // Logging and stats.
  base::Clock* const clock_;
  base::Timestamp last_recovered_packet_ = base::Timestamp::MinusInfinity();
  FecPacketCounter packet_counter_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_
