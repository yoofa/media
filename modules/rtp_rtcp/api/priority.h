/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_INCLUDE_PRIORITY_H_
#define AVE_MODULES_RTP_RTCP_INCLUDE_PRIORITY_H_

#include <stdint.h>

#include "base/checks.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// GENERATED_JAVA_ENUM_PACKAGE: org.webrtc
enum class Priority {
  kVeryLow,
  kLow,
  kMedium,
  kHigh,
};

class PriorityValue {
 public:
  explicit PriorityValue(Priority priority) {
    switch (priority) {
      case Priority::kVeryLow:
        value_ = 128;
        break;
      case Priority::kLow:
        value_ = 256;
        break;
      case Priority::kMedium:
        value_ = 512;
        break;
      case Priority::kHigh:
        value_ = 1024;
        break;
      default:
        AVE_CHECK(false);
    }
  }

  explicit PriorityValue(uint16_t priority) : value_(priority) {}

  uint16_t value() const { return value_; }

  bool operator==(const PriorityValue& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const PriorityValue& other) const {
    return !(*this == other);
  }
  bool operator<(const PriorityValue& other) const {
    return value_ < other.value_;
  }

 private:
  uint16_t value_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_INCLUDE_PRIORITY_H_
