/*
 * corruption_detection_extension.h - RTP Corruption Detection Header Extension
 * Ported from WebRTC
 *
 * Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 * Copyright (c) 2026 The aspect-oriented-express Authors. All rights reserved.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_CORRUPTION_DETECTION_EXTENSION_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_CORRUPTION_DETECTION_EXTENSION_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/types.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/util/corruption_detection_message.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// RTP Corruption Detection Header Extension.
//
// The class reads and writes the corruption detection RTP header extension.
// The class implements traits so that the class is compatible with being an
// argument to the templated `RtpPacket::GetExtension` and
// `RtpPacketToSend::SetExtension` methods.
class CorruptionDetectionExtension {
 public:
  using value_type = CorruptionDetectionMessage;

  static constexpr RTPExtensionType kId = kRtpExtensionCorruptionDetection;
  static constexpr uint8_t kMaxValueSizeBytes = 16;

  static constexpr std::string_view Uri() {
    return RtpExtension::kCorruptionDetectionUri;
  }
  static bool Parse(const uint8_t* data,
                    size_t size,
                    CorruptionDetectionMessage* message);
  static bool Write(uint8_t* data,
                    size_t size,
                    const CorruptionDetectionMessage& message);
  // Size of the header extension in bytes.
  static size_t ValueSize(const CorruptionDetectionMessage& message);
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_CORRUPTION_DETECTION_EXTENSION_H_
