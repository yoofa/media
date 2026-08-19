/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/rtp_timestamp_mapper.h"

#include "base/checks.h"

namespace ave::media::rtp_session {

RtpTimestampMapper::RtpTimestampMapper(uint32_t clock_rate_hz,
                                       uint32_t initial_timestamp)
    : clock_rate_hz_(clock_rate_hz), initial_timestamp_(initial_timestamp) {
  AVE_CHECK_GT(clock_rate_hz_, 0U);
}

uint32_t RtpTimestampMapper::Map(base::Timestamp pts) {
  if (!first_pts_.has_value()) {
    first_pts_ = pts;
    return initial_timestamp_;
  }
  return initial_timestamp_ +
         static_cast<uint32_t>(ToClockTicks(pts - *first_pts_));
}

void RtpTimestampMapper::Reset(uint32_t initial_timestamp) {
  initial_timestamp_ = initial_timestamp;
  first_pts_.reset();
}

int64_t RtpTimestampMapper::ToClockTicks(base::TimeDelta duration) const {
  constexpr int64_t kMicrosPerSecond = 1'000'000;
  const int64_t duration_us = duration.us();
  const int64_t whole_seconds = duration_us / kMicrosPerSecond;
  const int64_t remaining_us = duration_us % kMicrosPerSecond;
  return whole_seconds * static_cast<int64_t>(clock_rate_hz_) +
         remaining_us * static_cast<int64_t>(clock_rate_hz_) / kMicrosPerSecond;
}

}  // namespace ave::media::rtp_session
