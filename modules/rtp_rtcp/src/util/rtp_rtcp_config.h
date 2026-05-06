/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_CONFIG_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_CONFIG_H_

// Configuration file for RTP utilities (RTPSender, RTPReceiver ...)
namespace ave {
namespace media {
namespace rtp_rtcp {

constexpr int32_t kDefaultMaxReorderingThreshold = 50;  // In sequence numbers.
constexpr int32_t kRtcpMaxNackFields = 253;

constexpr int32_t RTCP_MAX_REPORT_BLOCKS = 31;  // RFC 3550 page 37

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_CONFIG_H_
