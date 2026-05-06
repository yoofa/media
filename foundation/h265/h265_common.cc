/*
 * h265_common.cc
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "media/foundation/h265/h265_common.h"
#include <span>

#include "media/foundation/h264/h264_common.h"

namespace ave {
namespace media {
namespace H265 {

namespace {
constexpr uint8_t kNaluTypeMask = 0x7E;

int32_t CountLeadingZeros32(uint32_t n) {
#ifdef __GNUC__
  return n == 0 ? 32 : __builtin_clz(n);
#else
  // Table used for counting leading zeros.
  static const int8_t kCountLeadingZeros32Table[64] = {
      32, 8,  17, -1, -1, 14, -1, -1, -1, 20, -1, -1, -1, 28, -1, 18,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  26, 25, 24,
      4,  11, 23, 31, 3,  7,  10, 16, 22, 30, -1, -1, 2,  6,  13, 9,
      -1, 15, -1, 21, -1, 29, 19, -1, -1, -1, -1, -1, 1,  27, 5,  12,
  };
  // Normalize n by rounding up to the nearest number that is a sequence of 0
  // bits followed by a sequence of 1 bits.
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return kCountLeadingZeros32Table[(n * 0x8c0b2891) >> 26];
#endif
}

}  // namespace

std::vector<NaluIndex> FindNaluIndices(std::span<const uint8_t> buffer) {
  std::vector<H264::NaluIndex> indices = H264::FindNaluIndices(buffer);
  std::vector<NaluIndex> results;
  for (auto& index : indices) {
    results.push_back({.start_offset = index.start_offset,
                       .payload_start_offset = index.payload_start_offset,
                       .payload_size = index.payload_size});
  }
  return results;
}

NaluType ParseNaluType(uint8_t data) {
  return static_cast<NaluType>((data & kNaluTypeMask) >> 1);
}

std::vector<uint8_t> ParseRbsp(std::span<const uint8_t> data) {
  return H264::ParseRbsp(data);
}

void WriteRbsp(std::span<const uint8_t> bytes, base::Buffer* destination) {
  H264::WriteRbsp(bytes, destination);
}

uint32_t Log2Ceiling(uint32_t value) {
  // When n == 0, we want the function to return -1.
  // When n == 0, (n - 1) will underflow to 0xFFFFFFFF, which is
  // why the statement below starts with (n ? 32 : -1).
  return (value ? 32 : -1) - CountLeadingZeros32(value - 1);
}

}  // namespace H265
}  // namespace media
}  // namespace ave
