/*
 * h265_sps_parser.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AVE_MEDIA_FOUNDATION_H265_H265_SPS_PARSER_H_
#define AVE_MEDIA_FOUNDATION_H265_H265_SPS_PARSER_H_

#include <optional>
#include <vector>

#include <span>
#include "base/buffer/bitstream_reader.h"

namespace ave {
namespace media {

// For explanations of each struct and its members, see H.265 specification
// at http://www.itu.int/rec/T-REC-H.265.
enum {
  kH265MaxLongTermRefPicSets = 32,   // 7.4.3.2.1
  kH265MaxShortTermRefPicSets = 64,  // 7.4.3.2.1
  kH265MaxSubLayers =
      7,  // 7.4.3.1 & 7.4.3.2.1 [v|s]ps_max_sub_layers_minus1 + 1
};

enum H265ProfileIdc {
  kH265ProfileIdcMain = 1,
  kH265ProfileIdcMain10 = 2,
  kH265ProfileIdcMainStill = 3,
  kH265ProfileIdcRangeExtensions = 4,
  kH265ProfileIdcHighThroughput = 5,
  kH265ProfileIdcMultiviewMain = 6,
  kH265ProfileIdcScalableMain = 7,
  kH265ProfileIdc3dMain = 8,
  kH265ProfileIdcScreenContentCoding = 9,
  kH265ProfileIdcScalableRangeExtensions = 10,
  kH265ProfileIdcHighThroughputScreenContentCoding = 11,
};

// A class for parsing out sequence parameter set (SPS) data from an H265 NALU.
class H265SpsParser {
 public:
  struct ProfileTierLevel {
    ProfileTierLevel();
    // Syntax elements.
    int general_profile_idc = 0;
    int general_level_idc = 0;  // 30x the actual level.
    uint32_t general_profile_compatibility_flags = 0;
    bool general_progressive_source_flag = false;
    bool general_interlaced_source_flag = false;
    bool general_non_packed_constraint_flag = false;
    bool general_frame_only_constraint_flag = false;
    bool general_one_picture_only_constraint_flag = false;
  };

  struct ShortTermRefPicSet {
    ShortTermRefPicSet();

    // Syntax elements.
    uint32_t num_negative_pics = 0;
    uint32_t num_positive_pics = 0;
    // NOLINTBEGIN(modernize-avoid-c-arrays)
    uint32_t delta_poc_s0[kH265MaxShortTermRefPicSets] = {};
    uint32_t used_by_curr_pic_s0[kH265MaxShortTermRefPicSets] = {};
    uint32_t delta_poc_s1[kH265MaxShortTermRefPicSets] = {};
    uint32_t used_by_curr_pic_s1[kH265MaxShortTermRefPicSets] = {};
    // NOLINTEND(modernize-avoid-c-arrays)

    // Calculated fields.
    uint32_t num_delta_pocs = 0;
  };

  // The parsed state of the SPS. Only some select values are stored.
  // Add more as they are actually needed.
  struct SpsState {
    SpsState() = default;

    uint32_t sps_max_sub_layers_minus1 = 0;
    uint32_t chroma_format_idc = 0;
    uint32_t separate_colour_plane_flag = 0;
    uint32_t pic_width_in_luma_samples = 0;
    uint32_t pic_height_in_luma_samples = 0;
    uint32_t log2_max_pic_order_cnt_lsb_minus4 = 0;
    // NOLINTNEXTLINE
    uint32_t sps_max_dec_pic_buffering_minus1[kH265MaxSubLayers] = {};
    uint32_t log2_min_luma_coding_block_size_minus3 = 0;
    uint32_t log2_diff_max_min_luma_coding_block_size = 0;
    uint32_t sample_adaptive_offset_enabled_flag = 0;
    uint32_t num_short_term_ref_pic_sets = 0;
    std::vector<H265SpsParser::ShortTermRefPicSet> short_term_ref_pic_set;
    uint32_t long_term_ref_pics_present_flag = 0;
    uint32_t num_long_term_ref_pics_sps = 0;
    std::vector<uint32_t> used_by_curr_pic_lt_sps_flag;
    uint32_t sps_temporal_mvp_enabled_flag = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t sps_id = 0;
    uint32_t vps_id = 0;
    uint32_t pic_width_in_ctbs_y = 0;
    uint32_t pic_height_in_ctbs_y = 0;
    uint32_t bit_depth_luma_minus8 = 0;
  };

  // Unpack RBSP and parse SPS state from the supplied buffer.
  static std::optional<SpsState> ParseSps(std::span<const uint8_t> data);

  static inline std::optional<SpsState> ParseSps(const uint8_t* data,
                                                 size_t length) {
    return ParseSps(std::span(data, length));
  }

  static bool ParseScalingListData(base::BitstreamReader& reader);

  static std::optional<ShortTermRefPicSet> ParseShortTermRefPicSet(
      uint32_t st_rps_idx,
      uint32_t num_short_term_ref_pic_sets,
      const std::vector<ShortTermRefPicSet>& short_term_ref_pic_sets,
      uint32_t sps_max_dec_pic_buffering_minus1,
      base::BitstreamReader& reader);

  static std::optional<H265SpsParser::ProfileTierLevel> ParseProfileTierLevel(
      bool profile_present,
      int max_num_sub_layers_minus1,
      base::BitstreamReader& reader);

 protected:
  // Parse the SPS state, for a bit buffer where RBSP decoding has already been
  // performed.
  static std::optional<SpsState> ParseSpsInternal(
      std::span<const uint8_t> buffer);

  // From Table A.8 - General tier and level limits.
  static int GetMaxLumaPs(int general_level_idc);
  // From A.4.2 - Profile-specific level limits for the video profiles.
  static size_t GetDpbMaxPicBuf(int general_profile_idc);
};

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_H265_H265_SPS_PARSER_H_
