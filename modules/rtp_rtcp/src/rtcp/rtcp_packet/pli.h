/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_

#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/psfb.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace rtcp {

class CommonHeader;

// Picture loss indication (PLI) (RFC 4585).
class Pli : public Psfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 1;

  Pli();
  Pli(const Pli& pli);
  ~Pli() override;

  bool Parse(const CommonHeader& packet);

  size_t BlockLength() const override;

  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              PacketReadyCallback callback) const override;
};

}  // namespace rtcp
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_
