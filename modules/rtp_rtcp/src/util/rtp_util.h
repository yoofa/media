/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTP_UTIL_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTP_UTIL_H_

#include <cstdint>

#include <span>

namespace ave {
namespace media {
namespace rtp_rtcp {

bool IsRtcpPacket(std::span<const uint8_t> packet);
bool IsRtpPacket(std::span<const uint8_t> packet);

// Returns base rtp header fields of the rtp packet.
// Behaviour is undefined when `!IsRtpPacket(rtp_packet)`.
int ParseRtpPayloadType(std::span<const uint8_t> rtp_packet);
uint16_t ParseRtpSequenceNumber(std::span<const uint8_t> rtp_packet);
uint32_t ParseRtpSsrc(std::span<const uint8_t> rtp_packet);

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTP_UTIL_H_
