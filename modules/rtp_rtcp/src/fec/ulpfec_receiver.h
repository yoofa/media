/*
 * ulpfec_receiver.h
 * Copyright (C) 2024 iWhaleshark <iwhalesharki@gmail.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ULPFEC_RECEIVER_H_
#define MODULES_RTP_RTCP_SOURCE_ULPFEC_RECEIVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/clock.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/recovered_packet_receiver.h"
#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include "media/modules/rtp_rtcp/src/fec/forward_error_correction.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

struct FecPacketCounter {
  FecPacketCounter() = default;
  size_t num_packets = 0;  // Number of received packets.
  size_t num_bytes = 0;
  size_t num_fec_packets = 0;  // Number of received FEC packets.
  size_t num_recovered_packets =
      0;  // Number of recovered media packets using FEC.
  // Time when first packet is received.
  base::Timestamp first_packet_time = base::Timestamp::MinusInfinity();
};

class UlpfecReceiver {
 public:
  UlpfecReceiver(uint32_t ssrc,
                 int ulpfec_payload_type,
                 RecoveredPacketReceiver* callback,
                 base::Clock* clock);
  ~UlpfecReceiver();

  int ulpfec_payload_type() const { return ulpfec_payload_type_; }

  bool AddReceivedRedPacket(const RtpPacketReceived& rtp_packet);

  void ProcessReceivedFec();

  FecPacketCounter GetPacketCounter() const;

 private:
  const uint32_t ssrc_;
  const int ulpfec_payload_type_;
  base::Clock* const clock_;

  RecoveredPacketReceiver* const recovered_packet_callback_;
  const std::unique_ptr<ForwardErrorCorrection> fec_;
  // TODO(nisse): The AddReceivedRedPacket method adds one or two packets to
  // this list at a time, after which it is emptied by ProcessReceivedFec. It
  // will make things simpler to merge AddReceivedRedPacket and
  // ProcessReceivedFec into a single method, and we can then delete this list.
  std::vector<std::unique_ptr<ForwardErrorCorrection::ReceivedPacket>>
      received_packets_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packets_;
  FecPacketCounter packet_counter_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MODULES_RTP_RTCP_SOURCE_ULPFEC_RECEIVER_H_
