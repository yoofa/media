/*
 * rtp_dependency_descriptor_extension.cc
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "media/modules/rtp_rtcp/src/rtp/rtp_dependency_descriptor_extension.h"

#include <bitset>
#include <cstdint>

#include <span>
#include "base/numerics/divide_round.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_dependency_descriptor_reader.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_dependency_descriptor_writer.h"
#include "media/modules/rtp_rtcp/src/util/dependency_descriptor.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

bool RtpDependencyDescriptorExtension::Parse(
    std::span<const uint8_t> data,
    const FrameDependencyStructure* structure,
    DependencyDescriptor* descriptor) {
  RtpDependencyDescriptorReader reader(data, structure, descriptor);
  return reader.ParseSuccessful();
}

size_t RtpDependencyDescriptorExtension::ValueSize(
    const FrameDependencyStructure& structure,
    std::bitset<32> active_chains,
    const DependencyDescriptor& descriptor) {
  RtpDependencyDescriptorWriter writer(/*data=*/{}, structure, active_chains,
                                       descriptor);
  return base::DivideRoundUp(writer.ValueSizeBits(), 8);
}

bool RtpDependencyDescriptorExtension::Write(
    std::span<uint8_t> data,
    const FrameDependencyStructure& structure,
    std::bitset<32> active_chains,
    const DependencyDescriptor& descriptor) {
  RtpDependencyDescriptorWriter writer(data, structure, active_chains,
                                       descriptor);
  return writer.Write();
}

bool RtpDependencyDescriptorExtension::Parse(
    std::span<const uint8_t> data,
    DependencyDescriptorMandatory* descriptor) {
  if (data.size() < 3) {
    return false;
  }
  descriptor->set_first_packet_in_frame(data[0] & 0b1000'0000);
  descriptor->set_last_packet_in_frame(data[0] & 0b0100'0000);
  descriptor->set_template_id(data[0] & 0b0011'1111);
  descriptor->set_frame_number((uint16_t{data[1]} << 8) | data[2]);
  return true;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
