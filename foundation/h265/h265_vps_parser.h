/*
 * h265_vps_parser.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AVE_MEDIA_FOUNDATION_H265_H265_VPS_PARSER_H_
#define AVE_MEDIA_FOUNDATION_H265_H265_VPS_PARSER_H_

#include <optional>

#include <span>

namespace ave {
namespace media {

// A class for parsing out video parameter set (VPS) data from an H265 NALU.
class H265VpsParser {
 public:
  // The parsed state of the VPS. Only some select values are stored.
  // Add more as they are actually needed.
  struct VpsState {
    VpsState();

    uint32_t id = 0;
  };

  // Unpack RBSP and parse VPS state from the supplied buffer.
  static std::optional<VpsState> ParseVps(std::span<const uint8_t> data);

  static inline std::optional<VpsState> ParseVps(const uint8_t* data,
                                                 size_t length) {
    return ParseVps(std::span(data, length));
  }

 protected:
  // Parse the VPS state, for a bit buffer where RBSP decoding has already been
  // performed.
  static std::optional<VpsState> ParseInternal(std::span<const uint8_t> buffer);
};

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_H265_H265_VPS_PARSER_H_
