/*
 * rtp_dependency_descriptor_reader.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include <span>
#include "base/buffer/bitstream_reader.h"
#include "media/modules/rtp_rtcp/src/util/dependency_descriptor.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Deserializes DependencyDescriptor rtp header extension.
class RtpDependencyDescriptorReader {
 public:
  // Parses the dependency descriptor.
  RtpDependencyDescriptorReader(std::span<const uint8_t> raw_data,
                                const FrameDependencyStructure* structure,
                                DependencyDescriptor* descriptor);
  RtpDependencyDescriptorReader(const RtpDependencyDescriptorReader&) = delete;
  RtpDependencyDescriptorReader& operator=(
      const RtpDependencyDescriptorReader&) = delete;

  // Returns true if parse was successful.
  bool ParseSuccessful() { return buffer_.Ok(); }

 private:
  // Functions to read template dependency structure.
  void ReadTemplateDependencyStructure();
  void ReadTemplateLayers();
  void ReadTemplateDtis();
  void ReadTemplateFdiffs();
  void ReadTemplateChains();
  void ReadResolutions();

  // Function to read details for the current frame.
  void ReadMandatoryFields();
  void ReadExtendedFields();
  void ReadFrameDependencyDefinition();

  void ReadFrameDtis();
  void ReadFrameFdiffs();
  void ReadFrameChains();

  // Output.
  DependencyDescriptor* const descriptor_;
  // Values that are needed while reading the descriptor, but can be discarded
  // when reading is complete.
  base::BitstreamReader buffer_;
  int32_t frame_dependency_template_id_ = 0;
  bool active_decode_targets_present_flag_ = false;
  bool custom_dtis_flag_ = false;
  bool custom_fdiffs_flag_ = false;
  bool custom_chains_flag_ = false;
  const FrameDependencyStructure* structure_ = nullptr;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_
