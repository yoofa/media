/*
 * h265_bitstream_parser.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AVE_MEDIA_FOUNDATION_H265_H265_BITSTREAM_PARSER_H_
#define AVE_MEDIA_FOUNDATION_H265_H265_BITSTREAM_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <vector>

#include <span>
#include "media/foundation/h265/h265_pps_parser.h"
#include "media/foundation/h265/h265_sps_parser.h"
#include "media/foundation/h265/h265_vps_parser.h"

namespace ave {
namespace media {

// Stateful H265 bitstream parser (due to VPS/SPS/PPS). Used to parse out QP
// values from the bitstream.
class H265BitstreamParser {
 public:
  H265BitstreamParser();
  ~H265BitstreamParser();

  // New interface.
  void ParseBitstream(std::span<const uint8_t> bitstream);
  std::optional<int32_t> GetLastSliceQp() const;

  std::optional<uint32_t> GetLastSlicePpsId() const;

  static std::optional<uint32_t> ParsePpsIdFromSliceSegmentLayerRbsp(
      std::span<const uint8_t> data,
      uint8_t nalu_type);

  // Returns true if the slice segment is the first in the picture; otherwise
  // return false. If parse failed, return nullopt.
  static std::optional<bool> IsFirstSliceSegmentInPic(
      std::span<const uint8_t> data);

 protected:
  enum Result {
    kOk,
    kInvalidStream,
    kUnsupportedStream,
  };
  void ParseSlice(std::span<const uint8_t> slice);
  Result ParseNonParameterSetNalu(std::span<const uint8_t> source,
                                  uint8_t nalu_type);

  const H265PpsParser::PpsState* GetPPS(uint32_t id) const;
  const H265SpsParser::SpsState* GetSPS(uint32_t id) const;

  // VPS/SPS/PPS state, updated when parsing new VPS/SPS/PPS, used to parse
  // slices.
  std::map<uint32_t, H265VpsParser::VpsState> vps_;
  std::map<uint32_t, H265SpsParser::SpsState> sps_;
  std::map<uint32_t, H265PpsParser::PpsState> pps_;

  // Last parsed slice QP.
  std::optional<int32_t> last_slice_qp_delta_;
  std::optional<uint32_t> last_slice_pps_id_;
};

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_H265_H265_BITSTREAM_PARSER_H_
