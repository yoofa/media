/*
 * dependency_descriptor.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_DEPENDENCY_DESCRIPTOR_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_DEPENDENCY_DESCRIPTOR_H_

#include <stdint.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <vector>

namespace ave {
namespace media {
namespace rtp_rtcp {

// Structures to build and parse dependency descriptor as described in
// https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension

// Simple render resolution class.
class RenderResolution {
 public:
  constexpr RenderResolution() = default;
  constexpr RenderResolution(int width, int height)
      : width_(width), height_(height) {}
  RenderResolution(const RenderResolution&) = default;
  RenderResolution& operator=(const RenderResolution&) = default;

  friend bool operator==(const RenderResolution& lhs,
                         const RenderResolution& rhs) {
    return lhs.width_ == rhs.width_ && lhs.height_ == rhs.height_;
  }
  friend bool operator!=(const RenderResolution& lhs,
                         const RenderResolution& rhs) {
    return !(lhs == rhs);
  }

  constexpr bool Valid() const { return width_ > 0 && height_ > 0; }

  constexpr int Width() const { return width_; }
  constexpr int Height() const { return height_; }

 private:
  int width_ = 0;
  int height_ = 0;
};

// Relationship of a frame to a Decode target.
enum class DecodeTargetIndication {
  kNotPresent = 0,   // DecodeTargetInfo symbol '-'
  kDiscardable = 1,  // DecodeTargetInfo symbol 'D'
  kSwitch = 2,       // DecodeTargetInfo symbol 'S'
  kRequired = 3      // DecodeTargetInfo symbol 'R'
};

struct FrameDependencyTemplate {
  // Setters are named briefly to chain them when building the template.
  FrameDependencyTemplate& S(int spatial_layer);
  FrameDependencyTemplate& T(int temporal_layer);
  FrameDependencyTemplate& Dtis(std::string_view dtis);
  FrameDependencyTemplate& FrameDiffs(std::initializer_list<int> diffs);
  FrameDependencyTemplate& ChainDiffs(std::initializer_list<int> diffs);

  friend bool operator==(const FrameDependencyTemplate& lhs,
                         const FrameDependencyTemplate& rhs) {
    return lhs.spatial_id == rhs.spatial_id &&
           lhs.temporal_id == rhs.temporal_id &&
           lhs.decode_target_indications == rhs.decode_target_indications &&
           lhs.frame_diffs == rhs.frame_diffs &&
           lhs.chain_diffs == rhs.chain_diffs;
  }

  int spatial_id = 0;
  int temporal_id = 0;
  std::vector<DecodeTargetIndication> decode_target_indications;
  std::vector<int> frame_diffs;
  std::vector<int> chain_diffs;
};

struct FrameDependencyStructure {
  friend bool operator==(const FrameDependencyStructure& lhs,
                         const FrameDependencyStructure& rhs) {
    return lhs.num_decode_targets == rhs.num_decode_targets &&
           lhs.num_chains == rhs.num_chains &&
           lhs.decode_target_protected_by_chain ==
               rhs.decode_target_protected_by_chain &&
           lhs.resolutions == rhs.resolutions && lhs.templates == rhs.templates;
  }

  int structure_id = 0;
  int num_decode_targets = 0;
  int num_chains = 0;
  // If chains are used (num_chains > 0), maps decode target index into index of
  // the chain protecting that target.
  std::vector<int> decode_target_protected_by_chain;
  std::vector<RenderResolution> resolutions;
  std::vector<FrameDependencyTemplate> templates;
};

class DependencyDescriptorMandatory {
 public:
  void set_frame_number(int frame_number) { frame_number_ = frame_number; }
  int frame_number() const { return frame_number_; }

  void set_template_id(int template_id) { template_id_ = template_id; }
  int template_id() const { return template_id_; }

  void set_first_packet_in_frame(bool first) { first_packet_in_frame_ = first; }
  bool first_packet_in_frame() const { return first_packet_in_frame_; }

  void set_last_packet_in_frame(bool last) { last_packet_in_frame_ = last; }
  bool last_packet_in_frame() const { return last_packet_in_frame_; }

 private:
  int frame_number_;
  int template_id_;
  bool first_packet_in_frame_;
  bool last_packet_in_frame_;
};

struct DependencyDescriptor {
  static constexpr int kMaxSpatialIds = 4;
  static constexpr int kMaxTemporalIds = 8;
  static constexpr int kMaxDecodeTargets = 32;
  static constexpr int kMaxTemplates = 64;

  bool first_packet_in_frame = true;
  bool last_packet_in_frame = true;
  int frame_number = 0;
  FrameDependencyTemplate frame_dependencies;
  std::optional<RenderResolution> resolution;
  std::optional<uint32_t> active_decode_targets_bitmask;
  std::unique_ptr<FrameDependencyStructure> attached_structure;
};

// Below are implementation details.
namespace impl {
std::vector<DecodeTargetIndication> StringToDecodeTargetIndications(
    std::string_view indication_symbols);
}  // namespace impl

inline FrameDependencyTemplate& FrameDependencyTemplate::S(int spatial_layer) {
  this->spatial_id = spatial_layer;
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::T(int temporal_layer) {
  this->temporal_id = temporal_layer;
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::Dtis(
    std::string_view dtis) {
  this->decode_target_indications = impl::StringToDecodeTargetIndications(dtis);
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::FrameDiffs(
    std::initializer_list<int> diffs) {
  this->frame_diffs.assign(diffs.begin(), diffs.end());
  return *this;
}
inline FrameDependencyTemplate& FrameDependencyTemplate::ChainDiffs(
    std::initializer_list<int> diffs) {
  this->chain_diffs.assign(diffs.begin(), diffs.end());
  return *this;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_DEPENDENCY_DESCRIPTOR_H_
