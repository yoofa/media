/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_
#define AVE_MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include <span>
#include "base/checks.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

using std::span;

class RtpHeaderExtensionMap {
 public:
  static constexpr RTPExtensionType kInvalidType = kRtpExtensionNone;
  static constexpr int kInvalidId = 0;

  RtpHeaderExtensionMap();
  explicit RtpHeaderExtensionMap(bool extmap_allow_mixed);
  explicit RtpHeaderExtensionMap(std::span<const RtpExtension> extensions);

  void Reset(std::span<const RtpExtension> extensions);

  template <typename Extension>
  bool Register(int id) {
    return Register(id, Extension::kId, Extension::Uri());
  }
  bool RegisterByType(int id, RTPExtensionType type);
  bool RegisterByUri(int id, std::string_view uri);

  bool IsRegistered(RTPExtensionType type) const {
    return GetId(type) != kInvalidId;
  }
  // Return kInvalidType if not found.
  RTPExtensionType GetType(int id) const;
  // Return kInvalidId if not found.
  uint8_t GetId(RTPExtensionType type) const {
    AVE_DCHECK_GT(type, kRtpExtensionNone);
    AVE_DCHECK_LT(type, kRtpExtensionNumberOfExtensions);
    return ids_[type];
  }

  void Deregister(std::string_view uri);

  // Corresponds to the SDP attribute extmap-allow-mixed, see RFC8285.
  // Set to true if it's allowed to mix one- and two-byte RTP header extensions
  // in the same stream.
  bool ExtmapAllowMixed() const { return extmap_allow_mixed_; }
  void SetExtmapAllowMixed(bool extmap_allow_mixed) {
    extmap_allow_mixed_ = extmap_allow_mixed;
  }

 private:
  bool Register(int id, RTPExtensionType type, std::string_view uri);

  uint8_t ids_[kRtpExtensionNumberOfExtensions];
  bool extmap_allow_mixed_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_
