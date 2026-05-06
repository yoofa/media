/*
 * report_block_data.cc - RTCP Report Block Data Implementation
 * Ported from WebRTC (modules/rtp_rtcp/include/report_block_data.cc)
 *
 * Copyright (c) 2019 The WebRTC Project Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the root of the source tree. An additional
 * intellectual property rights grant can be found in the file PATENTS.
 */

#include "media/modules/rtp_rtcp/api/report_block_data.h"

#include "base/checks.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

using base::TimeDelta;

TimeDelta ReportBlockData::jitter(int32_t rtp_clock_rate_hz) const {
  AVE_DCHECK_GT(rtp_clock_rate_hz, 0);
  // Conversion to TimeDelta and division are swapped to avoid conversion
  // to/from floating point types.
  return TimeDelta::Seconds(jitter()) / rtp_clock_rate_hz;
}

void ReportBlockData::SetReportBlock(uint32_t sender_ssrc,
                                     const ReportBlock& report_block,
                                     Timestamp report_block_timestamp_utc,
                                     Timestamp report_block_timestamp) {
  sender_ssrc_ = sender_ssrc;
  source_ssrc_ = report_block.source_ssrc();
  fraction_lost_raw_ = report_block.fraction_lost();
  cumulative_lost_ = report_block.cumulative_lost();
  extended_highest_sequence_number_ = report_block.extended_high_seq_num();
  jitter_ = report_block.jitter();
  report_block_timestamp_utc_ = report_block_timestamp_utc;
  report_block_timestamp_ = report_block_timestamp;
}

void ReportBlockData::AddRoundTripTimeSample(TimeDelta rtt) {
  last_rtt_ = rtt;
  sum_rtt_ += rtt;
  ++num_rtts_;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
