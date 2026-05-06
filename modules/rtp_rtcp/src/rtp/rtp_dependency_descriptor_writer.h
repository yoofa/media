/*
 * rtp_dependency_descriptor_writer.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <span>
#include "base/buffer/bit_buffer.h"
#include "media/modules/rtp_rtcp/src/util/dependency_descriptor.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpDependencyDescriptorWriter {
 public:
  // Assumes `structure` and `descriptor` are valid and
  // `descriptor` matches the `structure`.
  RtpDependencyDescriptorWriter(std::span<uint8_t> data,
                                const FrameDependencyStructure& structure,
                                std::bitset<32> active_chains,
                                const DependencyDescriptor& descriptor);

  // Serializes DependencyDescriptor rtp header extension.
  // Returns false if `data` is too small to serialize the `descriptor`.
  bool Write();

  // Returns minimum number of bits needed to serialize descriptor with respect
  // to the `structure`. Returns 0 if `descriptor` can't be serialized.
  int32_t ValueSizeBits() const;

 private:
  // Used both as pointer to the template and as index in the templates vector.
  using TemplateIterator = std::vector<FrameDependencyTemplate>::const_iterator;
  struct TemplateMatch {
    TemplateIterator template_position;
    bool need_custom_dtis;
    bool need_custom_fdiffs;
    bool need_custom_chains;
    // Size in bits to store frame-specific details, i.e.
    // excluding mandatory fields and template dependency structure.
    int32_t extra_size_bits;
  };
  int32_t StructureSizeBits() const;
  TemplateMatch CalculateMatch(TemplateIterator frame_template) const;
  void FindBestTemplate();
  bool ShouldWriteActiveDecodeTargetsBitmask() const;
  bool HasExtendedFields() const;
  uint64_t TemplateId() const;

  void WriteBits(uint64_t val, size_t bit_count);
  void WriteNonSymmetric(uint32_t value, uint32_t num_values);

  // Functions to read template dependency structure.
  void WriteTemplateDependencyStructure();
  void WriteTemplateLayers();
  void WriteTemplateDtis();
  void WriteTemplateFdiffs();
  void WriteTemplateChains();
  void WriteResolutions();

  // Function to read details for the current frame.
  void WriteMandatoryFields();
  void WriteExtendedFields();
  void WriteFrameDependencyDefinition();

  void WriteFrameDtis();
  void WriteFrameFdiffs();
  void WriteFrameChains();

  bool build_failed_ = false;
  const DependencyDescriptor& descriptor_;
  const FrameDependencyStructure& structure_;
  std::bitset<32> active_chains_;
  base::BitBufferWriter bit_writer_;
  TemplateMatch best_template_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_
