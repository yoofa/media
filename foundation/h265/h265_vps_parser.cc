/*
 * h265_vps_parser.cc
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "media/foundation/h265/h265_vps_parser.h"
#include <span>

#include "base/buffer/bitstream_reader.h"
#include "media/foundation/h265/h265_common.h"

namespace ave {
namespace media {

H265VpsParser::VpsState::VpsState() = default;

// General note: this is based off the 08/2021 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse VPS state from the supplied buffer.
std::optional<H265VpsParser::VpsState> H265VpsParser::ParseVps(
    std::span<const uint8_t> data) {
  std::vector<uint8_t> rbsp = H265::ParseRbsp(data);
  return ParseInternal(std::span<const uint8_t>(rbsp.data(), rbsp.size()));
}

std::optional<H265VpsParser::VpsState> H265VpsParser::ParseInternal(
    std::span<const uint8_t> buffer) {
  base::BitstreamReader reader(buffer);

  // Now, we need to use a bit buffer to parse through the actual H265 VPS
  // format. See Section 7.3.2.1 ("Video parameter set RBSP syntax") of the
  // H.265 standard for a complete description.
  VpsState vps;

  // vps_video_parameter_set_id: u(4)
  vps.id = static_cast<uint32_t>(reader.ReadBits(4));

  if (!reader.Ok()) {
    return std::nullopt;
  }

  return vps;
}

}  // namespace media
}  // namespace ave
