/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_

#include <cstdint>
#include <utility>

#include <memory>
#include <span>
#include "base/net/ecn_marking.h"

#include <memory>

#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_headers.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace base = ::ave::base;
using ::ave::base::EcnMarking;

// Class to hold rtp packet with metadata for receiver side.
// The metadata is not parsed from the rtp packet, but may be derived from the
// data that is parsed from the rtp packet.
class RtpPacketReceived : public RtpPacket {
 public:
  RtpPacketReceived();
  explicit RtpPacketReceived(
      const ExtensionManager* extensions,
      base::Timestamp arrival_time = base::Timestamp::MinusInfinity());
  RtpPacketReceived(const RtpPacketReceived& packet);
  RtpPacketReceived(RtpPacketReceived&& packet);

  RtpPacketReceived& operator=(const RtpPacketReceived& packet);
  RtpPacketReceived& operator=(RtpPacketReceived&& packet);

  ~RtpPacketReceived();

  // TODO(bugs.webrtc.org/15054): Remove this function when all code is updated
  // to use RtpPacket directly.
  void GetHeader(RTPHeader* header) const;

  // Time in local time base as close as it can to packet arrived on the
  // network.
  base::Timestamp arrival_time() const { return arrival_time_; }
  void set_arrival_time(base::Timestamp time) { arrival_time_ = time; }

  // Explicit Congestion Notification (ECN), RFC-3168, Section 5.
  // Used by L4S: https://www.rfc-editor.org/rfc/rfc9331.html
  EcnMarking ecn() const { return ecn_; }
  void set_ecn(EcnMarking ecn) { ecn_ = ecn; }

  // Flag if packet was recovered via RTX or FEC.
  bool recovered() const { return recovered_; }
  void set_recovered(bool value) { recovered_ = value; }

  int payload_type_frequency() const { return payload_type_frequency_; }
  void set_payload_type_frequency(int value) {
    payload_type_frequency_ = value;
  }

  // An application can attach arbitrary data to an RTP packet using
  // `additional_data`. The additional data does not affect WebRTC processing.
  std::shared_ptr<void> additional_data() const { return additional_data_; }
  void set_additional_data(std::shared_ptr<void> data) {
    additional_data_ = std::move(data);
  }

 private:
  base::Timestamp arrival_time_ = base::Timestamp::MinusInfinity();
  EcnMarking ecn_ = EcnMarking::kNotEct;
  int payload_type_frequency_ = 0;
  bool recovered_ = false;
  std::shared_ptr<void> additional_data_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_
