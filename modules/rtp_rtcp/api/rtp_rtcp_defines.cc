/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

#include <string.h>
#include <algorithm>
#include <cctype>

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace {
constexpr size_t kMidRsidMaxSize = 16;

// Check if passed character is a "token-char" from RFC 4566.
// https://datatracker.ietf.org/doc/html/rfc4566#section-9
//    token-char =          %x21 / %x23-27 / %x2A-2B / %x2D-2E / %x30-39
//                         / %x41-5A / %x5E-7E
bool IsTokenChar(char ch) {
  return ch == 0x21 || (ch >= 0x23 && ch <= 0x27) || ch == 0x2a || ch == 0x2b ||
         ch == 0x2d || ch == 0x2e || (ch >= 0x30 && ch <= 0x39) ||
         (ch >= 0x41 && ch <= 0x5a) || (ch >= 0x5e && ch <= 0x7e);
}
}  // namespace

bool IsLegalMidName(std::string_view name) {
  return (name.size() <= kMidRsidMaxSize && !name.empty() &&
          std::all_of(name.begin(), name.end(), IsTokenChar));
}

bool IsLegalRsidName(std::string_view name) {
  return (name.size() <= kMidRsidMaxSize && !name.empty() &&
          std::all_of(name.begin(), name.end(),
                      [](char c) { return std::isalnum(c); }));
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
