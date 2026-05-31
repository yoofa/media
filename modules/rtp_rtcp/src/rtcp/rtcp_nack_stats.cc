/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/src/rtcp/rtcp_nack_stats.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace {
// Returns true if `seq_num` is newer than `prev_seq_num`.
inline bool IsNewerSequenceNumber(uint16_t seq_num, uint16_t prev_seq_num) {
  return static_cast<int16_t>(seq_num - prev_seq_num) > 0;
}
}  // namespace

RtcpNackStats::RtcpNackStats()
    : max_sequence_number_(0), requests_(0), unique_requests_(0) {}

void RtcpNackStats::ReportRequest(uint16_t sequence_number) {
  if (requests_ == 0 ||
      IsNewerSequenceNumber(sequence_number, max_sequence_number_)) {
    max_sequence_number_ = sequence_number;
    ++unique_requests_;
  }
  ++requests_;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
