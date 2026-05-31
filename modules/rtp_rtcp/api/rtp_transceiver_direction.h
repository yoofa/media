/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_INCLUDE_RTP_TRANSCEIVER_DIRECTION_H_
#define AVE_MODULES_RTP_RTCP_INCLUDE_RTP_TRANSCEIVER_DIRECTION_H_

namespace ave {
namespace media {
namespace rtp_rtcp {

// https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiverdirection
enum class RtpTransceiverDirection {
  kSendRecv,
  kSendOnly,
  kRecvOnly,
  kInactive,
  kStopped,
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_INCLUDE_RTP_TRANSCEIVER_DIRECTION_H_
