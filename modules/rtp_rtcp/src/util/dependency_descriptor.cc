/*
 * dependency_descriptor.cc
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "media/modules/rtp_rtcp/src/util/dependency_descriptor.h"

#include <string_view>

#include "absl/container/inlined_vector.h"
#include "base/checks.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

constexpr int32_t DependencyDescriptor::kMaxSpatialIds;
constexpr int32_t DependencyDescriptor::kMaxTemporalIds;
constexpr int32_t DependencyDescriptor::kMaxTemplates;
constexpr int32_t DependencyDescriptor::kMaxDecodeTargets;

namespace impl {

std::vector<DecodeTargetIndication> StringToDecodeTargetIndications(
    std::string_view symbols) {
  std::vector<DecodeTargetIndication> dtis;
  dtis.reserve(symbols.size());
  for (char symbol : symbols) {
    DecodeTargetIndication indication;
    switch (symbol) {
      case '-':
        indication = DecodeTargetIndication::kNotPresent;
        break;
      case 'D':
        indication = DecodeTargetIndication::kDiscardable;
        break;
      case 'R':
        indication = DecodeTargetIndication::kRequired;
        break;
      case 'S':
        indication = DecodeTargetIndication::kSwitch;
        break;
      default:
        AVE_DCHECK(false) << "Unknown indication symbol: " << symbol;
        indication = DecodeTargetIndication::kNotPresent;
    }
    dtis.push_back(indication);
  }
  return dtis;
}

}  // namespace impl
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
