/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/src/util/leb128.h"

#include <cstdint>

namespace ave {
namespace media {
namespace rtp_rtcp {

int32_t Leb128Size(uint64_t value) {
  int32_t size = 0;
  while (value >= 0x80) {
    ++size;
    value >>= 7;
  }
  return size + 1;
}

uint64_t ReadLeb128(const uint8_t*& read_at, const uint8_t* end) {
  uint64_t value = 0;
  int32_t fill_bits = 0;
  while (read_at != end && fill_bits < 64 - 7) {
    uint8_t leb128_byte = *read_at;
    value |= uint64_t{leb128_byte & 0x7Fu} << fill_bits;
    ++read_at;
    fill_bits += 7;
    if ((leb128_byte & 0x80) == 0) {
      return value;
    }
  }
  // Read 9 bytes and didn't find the terminator byte. Check if 10th byte
  // is that terminator, however to fit result into uint64_t it may carry only
  // single bit.
  if (read_at != end && *read_at <= 1) {
    value |= uint64_t{*read_at} << fill_bits;
    ++read_at;
    return value;
  }
  // Failed to find terminator leb128 byte.
  read_at = nullptr;
  return 0;
}

int32_t WriteLeb128(uint64_t value, uint8_t* buffer) {
  int32_t size = 0;
  while (value >= 0x80) {
    buffer[size] = 0x80 | (value & 0x7F);
    ++size;
    value >>= 7;
  }
  buffer[size] = value;
  ++size;
  return size;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
