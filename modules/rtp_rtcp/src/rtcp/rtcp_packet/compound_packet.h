/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_

#include <memory>
#include <vector>

#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace rtcp {

class CompoundPacket : public RtcpPacket {
 public:
  CompoundPacket();
  ~CompoundPacket() override;

  CompoundPacket(const CompoundPacket&) = delete;
  CompoundPacket& operator=(const CompoundPacket&) = delete;

  void Append(std::unique_ptr<RtcpPacket> packet);

  // Size of this packet in bytes (i.e. total size of nested packets).
  size_t BlockLength() const override;
  // Returns true if all calls to Create succeeded.
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              PacketReadyCallback callback) const override;

 protected:
  std::vector<std::unique_ptr<RtcpPacket>> appended_packets_;
};

}  // namespace rtcp
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_
