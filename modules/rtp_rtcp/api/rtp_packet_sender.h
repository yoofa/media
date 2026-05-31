/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_
#define AVE_MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_

#include <cstdint>
#include <memory>
#include <vector>

namespace ave {
namespace media {
namespace rtp_rtcp {

// Forward declaration
class RtpPacketToSend;

class RtpPacketSender {
 public:
  virtual ~RtpPacketSender() = default;

  // Insert a set of packets into queue, for eventual transmission. Based on the
  // type of packets, they will be prioritized and scheduled relative to other
  // packets and the current target send rate.
  virtual void EnqueuePackets(
      std::vector<std::unique_ptr<RtpPacketToSend>> packets) = 0;

  // Clear any pending packets with the given SSRC from the queue.
  virtual void RemovePacketsForSsrc(uint32_t /* ssrc */) {}
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_
