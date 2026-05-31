/*
 * rtp_dependency_descriptor_extension.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_EXTENSION_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_EXTENSION_H_

#include <bitset>
#include <cstdint>
#include <string_view>

#include <span>
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/util/dependency_descriptor.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Trait to read/write the dependency descriptor extension as described in
// https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension
class RtpDependencyDescriptorExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionDependencyDescriptor;
  static constexpr std::string_view Uri() {
    return RtpExtension::kDependencyDescriptorUri;
  }

  static bool Parse(std::span<const uint8_t> data,
                    const FrameDependencyStructure* structure,
                    DependencyDescriptor* descriptor);

  // Reads the mandatory part of the descriptor.
  // Such read is stateless, i.e., doesn't require `FrameDependencyStructure`.
  static bool Parse(std::span<const uint8_t> data,
                    DependencyDescriptorMandatory* descriptor);

  static size_t ValueSize(const FrameDependencyStructure& structure,
                          const DependencyDescriptor& descriptor) {
    return ValueSize(structure, kAllChainsAreActive, descriptor);
  }
  static size_t ValueSize(const FrameDependencyStructure& structure,
                          std::bitset<32> active_chains,
                          const DependencyDescriptor& descriptor);
  static bool Write(std::span<uint8_t> data,
                    const FrameDependencyStructure& structure,
                    const DependencyDescriptor& descriptor) {
    return Write(data, structure, kAllChainsAreActive, descriptor);
  }
  static bool Write(std::span<uint8_t> data,
                    const FrameDependencyStructure& structure,
                    std::bitset<32> active_chains,
                    const DependencyDescriptor& descriptor);

 private:
  static constexpr std::bitset<32> kAllChainsAreActive = ~uint32_t{0};
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_EXTENSION_H_
