/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_

#include <stdint.h>

#include <vector>

#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/tmmb_item.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class TMMBRHelp {
 public:
  static std::vector<rtcp::TmmbItem> FindBoundingSet(
      std::vector<rtcp::TmmbItem> candidates);

  static bool IsOwner(const std::vector<rtcp::TmmbItem>& bounding,
                      uint32_t ssrc);

  static uint64_t CalcMinBitrateBps(
      const std::vector<rtcp::TmmbItem>& candidates);
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
