/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/api/rtp_packet_info.h"

#include <cstddef>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_headers.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

RtpPacketInfo::RtpPacketInfo()
    : ssrc_(0),
      rtp_timestamp_(0),
      receive_time_(base::Timestamp::MinusInfinity()) {}

RtpPacketInfo::RtpPacketInfo(uint32_t ssrc,
                             std::vector<uint32_t> csrcs,
                             uint32_t rtp_timestamp,
                             base::Timestamp receive_time)
    : ssrc_(ssrc),
      csrcs_(std::move(csrcs)),
      rtp_timestamp_(rtp_timestamp),
      receive_time_(receive_time) {}

RtpPacketInfo::RtpPacketInfo(const RTPHeader& rtp_header,
                             base::Timestamp receive_time)
    : ssrc_(rtp_header.ssrc),
      rtp_timestamp_(rtp_header.timestamp),
      receive_time_(receive_time) {
  const auto& extension = rtp_header.extension;
  const auto csrcs_count = std::min<size_t>(rtp_header.numCSRCs, kRtpCsrcSize);

  csrcs_.assign(&rtp_header.arrOfCSRCs[0], &rtp_header.arrOfCSRCs[csrcs_count]);

  if (extension.audio_level()) {
    audio_level_ = extension.audio_level()->level();
  }

  absolute_capture_time_ = extension.absolute_capture_time;
}

bool operator==(const RtpPacketInfo& lhs, const RtpPacketInfo& rhs) {
  return (lhs.ssrc() == rhs.ssrc()) && (lhs.csrcs() == rhs.csrcs()) &&
         (lhs.rtp_timestamp() == rhs.rtp_timestamp()) &&
         (lhs.receive_time() == rhs.receive_time()) &&
         (lhs.audio_level() == rhs.audio_level()) &&
         (lhs.absolute_capture_time() == rhs.absolute_capture_time()) &&
         (lhs.local_capture_clock_offset() == rhs.local_capture_clock_offset());
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
