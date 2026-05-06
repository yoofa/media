/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_

#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/common_header.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/psfb.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace rtcp {

class LossNotification : public Psfb {
 public:
  LossNotification();
  LossNotification(uint16_t last_decoded,
                   uint16_t last_received,
                   bool decodability_flag);
  LossNotification(const LossNotification& rhs);
  ~LossNotification() override;

  size_t BlockLength() const override;

  [[nodiscard]] bool Create(uint8_t* packet,
                            size_t* index,
                            size_t max_length,
                            PacketReadyCallback callback) const override;

  // Parse assumes header is already parsed and validated.
  [[nodiscard]] bool Parse(const CommonHeader& packet);

  // Set all of the values transmitted by the loss notification message.
  // If the values may not be represented by a loss notification message,
  // false is returned, and no change is made to the object; this happens
  // when `last_received` is ahead of `last_decoded` by more than 0x7fff.
  // This is because `last_received` is represented on the wire as a delta,
  // and only 15 bits are available for that delta.
  [[nodiscard]] bool Set(uint16_t last_decoded,
                         uint16_t last_received,
                         bool decodability_flag);

  // RTP sequence number of the first packet belong to the last decoded
  // non-discardable frame.
  uint16_t last_decoded() const { return last_decoded_; }

  // RTP sequence number of the last received packet.
  uint16_t last_received() const { return last_received_; }

  // A decodability flag, whose specific meaning depends on the last-received
  // RTP sequence number.
  bool decodability_flag() const { return decodability_flag_; }

 private:
  static constexpr uint32_t kUniqueIdentifier = 0x4C4E5446;  // 'L' 'N' 'T' 'F'.
  static constexpr size_t kLossNotificationPayloadLength = 8;

  uint16_t last_decoded_;
  uint16_t last_received_;
  bool decodability_flag_;
};

}  // namespace rtcp
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_
