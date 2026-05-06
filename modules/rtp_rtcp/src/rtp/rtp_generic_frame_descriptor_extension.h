/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef AVE_MODULES_RTP_RTCP_SOURCE_RTP_GENERIC_FRAME_DESCRIPTOR_EXTENSION_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_RTP_GENERIC_FRAME_DESCRIPTOR_EXTENSION_H_

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include <span>
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_generic_frame_descriptor.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Trait to read/write the generic frame descriptor, the early version of the
// dependency descriptor extension. Usage of this rtp header extension is
// discouraged in favor of the dependency descriptor.
class RtpGenericFrameDescriptorExtension00 {
 public:
  using value_type = RtpGenericFrameDescriptor;
  static constexpr RTPExtensionType kId = kRtpExtensionGenericFrameDescriptor;
  static constexpr std::string_view Uri() {
    return RtpExtension::kGenericFrameDescriptorUri00;
  }
  static constexpr int32_t kMaxSizeBytes = 16;

  static bool Parse(std::span<const uint8_t> data,
                    RtpGenericFrameDescriptor* descriptor);
  static size_t ValueSize(const RtpGenericFrameDescriptor& descriptor);
  static bool Write(std::span<uint8_t> data,
                    const RtpGenericFrameDescriptor& descriptor);
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_RTP_GENERIC_FRAME_DESCRIPTOR_EXTENSION_H_
