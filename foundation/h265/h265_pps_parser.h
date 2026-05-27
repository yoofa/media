/*
 * h265_pps_parser.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AVE_MEDIA_FOUNDATION_H265_H265_PPS_PARSER_H_
#define AVE_MEDIA_FOUNDATION_H265_H265_PPS_PARSER_H_

#include <optional>
#include <span>

#include "base/buffer/bitstream_reader.h"
#include "media/foundation/h265/h265_sps_parser.h"

namespace ave {
namespace media {

// A class for parsing out picture parameter set (PPS) data from a H265 NALU.
class H265PpsParser {
 public:
  // The parsed state of the PPS. Only some select values are stored.
  // Add more as they are actually needed.
  struct PpsState {
    PpsState() = default;

    bool dependent_slice_segments_enabled_flag = false;
    bool cabac_init_present_flag = false;
    bool output_flag_present_flag = false;
    uint32_t num_extra_slice_header_bits = 0;
    uint32_t num_ref_idx_l0_default_active_minus1 = 0;
    uint32_t num_ref_idx_l1_default_active_minus1 = 0;
    int32_t init_qp_minus26 = 0;
    bool weighted_pred_flag = false;
    bool weighted_bipred_flag = false;
    bool lists_modification_present_flag = false;
    uint32_t pps_id = 0;
    uint32_t sps_id = 0;
    int32_t qp_bd_offset_y = 0;
  };

  // Unpack RBSP and parse PPS state from the supplied buffer.
  static std::optional<PpsState> ParsePps(std::span<const uint8_t> data,
                                          const H265SpsParser::SpsState* sps);

  static inline std::optional<PpsState> ParsePps(
      const uint8_t* data,
      size_t length,
      const H265SpsParser::SpsState* sps) {
    return ParsePps(std::span(data, length), sps);
  }

  static bool ParsePpsIds(std::span<const uint8_t> data,
                          uint32_t* pps_id,
                          uint32_t* sps_id);

  static inline bool ParsePpsIds(const uint8_t* data,
                                 size_t length,
                                 uint32_t* pps_id,
                                 uint32_t* sps_id) {
    return ParsePpsIds(std::span(data, length), pps_id, sps_id);
  }

 protected:
  // Parse the PPS state, for a bit buffer where RBSP decoding has already been
  // performed.
  static std::optional<PpsState> ParseInternal(
      std::span<const uint8_t> buffer,
      const H265SpsParser::SpsState* sps);
  static bool ParsePpsIdsInternal(base::BitstreamReader& reader,
                                  uint32_t& pps_id,
                                  uint32_t& sps_id);
};

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_H265_H265_PPS_PARSER_H_
