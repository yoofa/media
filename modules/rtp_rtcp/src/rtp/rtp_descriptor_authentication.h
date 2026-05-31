/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Ported to AVE from WebRTC.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DESCRIPTOR_AUTHENTICATION_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DESCRIPTOR_AUTHENTICATION_H_

#include <cstdint>
#include <vector>

#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Converts frame dependencies into array of bytes for authentication.
std::vector<uint8_t> RtpDescriptorAuthentication(
    const RTPVideoHeader& rtp_video_header);

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DESCRIPTOR_AUTHENTICATION_H_
