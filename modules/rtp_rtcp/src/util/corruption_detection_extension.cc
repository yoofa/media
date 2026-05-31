/*
 * corruption_detection_extension.cc - RTP Corruption Detection Header Extension
 * Ported from WebRTC
 *
 * Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 * Copyright (c) 2026 The aspect-oriented-express Authors. All rights reserved.
 */

#include "media/modules/rtp_rtcp/src/util/corruption_detection_extension.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace {

constexpr size_t kMandatoryPayloadBytes = 1;
constexpr size_t kConfigurationBytes = 3;
constexpr double kMaxValueForStdDev = 40.0;

}  // namespace

// A description of the extension can be found at
// http://www.webrtc.org/experiments/rtp-hdrext/corruption-detection

bool CorruptionDetectionExtension::Parse(const uint8_t* data,
                                         size_t size,
                                         CorruptionDetectionMessage* message) {
  if (message == nullptr) {
    return false;
  }
  if ((size != kMandatoryPayloadBytes && size <= kConfigurationBytes) ||
      size > kMaxValueSizeBytes) {
    return false;
  }
  message->interpret_sequence_index_as_most_significant_bits_ = data[0] >> 7;
  message->sequence_index_ = data[0] & 0b0111'1111;
  if (size == kMandatoryPayloadBytes) {
    return true;
  }
  message->std_dev_ = data[1] * kMaxValueForStdDev / 255.0;
  uint8_t channel_error_thresholds = data[2];
  message->luma_error_threshold_ = channel_error_thresholds >> 4;
  message->chroma_error_threshold_ = channel_error_thresholds & 0xF;
  message->sample_values_.assign(data + kConfigurationBytes, data + size);
  return true;
}

bool CorruptionDetectionExtension::Write(
    uint8_t* data,
    size_t size,
    const CorruptionDetectionMessage& message) {
  if (size != ValueSize(message) || size > kMaxValueSizeBytes) {
    return false;
  }

  data[0] = message.sequence_index() & 0b0111'1111;
  if (message.interpret_sequence_index_as_most_significant_bits()) {
    data[0] |= 0b1000'0000;
  }
  if (message.sample_values().empty()) {
    return true;
  }
  data[1] = static_cast<uint8_t>(
      std::round(message.std_dev() / kMaxValueForStdDev * 255.0));
  data[2] = (message.luma_error_threshold() << 4) |
            (message.chroma_error_threshold() & 0xF);
  uint8_t* sample_data = data + kConfigurationBytes;
  for (size_t i = 0; i < message.sample_values().size(); ++i) {
    sample_data[i] =
        static_cast<uint8_t>(std::floor(message.sample_values()[i]));
  }
  return true;
}

size_t CorruptionDetectionExtension::ValueSize(
    const CorruptionDetectionMessage& message) {
  if (message.sample_values_.empty()) {
    return kMandatoryPayloadBytes;
  }
  return kConfigurationBytes + message.sample_values_.size();
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
