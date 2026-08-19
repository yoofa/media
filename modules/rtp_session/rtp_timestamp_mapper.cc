/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/rtp_timestamp_mapper.h"

#include "base/checks.h"
#include "base/logging.h"

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

RtpReceiveTimestampMapper::RtpReceiveTimestampMapper(Config config)
    : config_(config) {
  AVE_CHECK_GT(config_.clock_rate_hz, 0U);
  AVE_CHECK_GT(config_.maximum_continuous_gap_ms, 0U);
  AVE_CHECK_GT(config_.restart_gap_us, 0U);
}

base::Timestamp RtpReceiveTimestampMapper::Map(uint32_t rtp_timestamp) {
  int64_t unwrapped = timestamp_unwrapper_.Unwrap(rtp_timestamp);
  const int64_t maximum_gap_ticks =
      static_cast<int64_t>(config_.clock_rate_hz) *
      config_.maximum_continuous_gap_ms / 1000;
  if (last_rtp_timestamp_.has_value()) {
    const int64_t delta_ticks = unwrapped - *last_rtp_timestamp_;
    if (delta_ticks < 0 || delta_ticks > maximum_gap_ticks) {
      AVE_LOG(LS_WARNING) << "Resetting discontinuous RTP receive timeline: "
                          << delta_ticks << " ticks";
      timestamp_unwrapper_.Reset();
      unwrapped = timestamp_unwrapper_.Unwrap(rtp_timestamp);
      first_rtp_timestamp_ = unwrapped;
      pts_epoch_us_ =
          last_pts_us_ < 0 ? 0 : last_pts_us_ + config_.restart_gap_us;
    }
  }
  if (!first_rtp_timestamp_.has_value()) {
    first_rtp_timestamp_ = unwrapped;
  }

  int64_t pts_us = pts_epoch_us_ + ToMicros(unwrapped - *first_rtp_timestamp_);
  if (last_pts_us_ >= 0 && pts_us <= last_pts_us_) {
    pts_us = last_pts_us_ + 1;
  }
  last_rtp_timestamp_ = unwrapped;
  last_pts_us_ = pts_us;
  return base::Timestamp::Micros(pts_us);
}

void RtpReceiveTimestampMapper::Reset() {
  timestamp_unwrapper_.Reset();
  first_rtp_timestamp_.reset();
  last_rtp_timestamp_.reset();
  pts_epoch_us_ = 0;
  last_pts_us_ = -1;
}

int64_t RtpReceiveTimestampMapper::ToMicros(int64_t ticks) const {
  constexpr int64_t kMicrosPerSecond = 1'000'000;
  const int64_t clock_rate = static_cast<int64_t>(config_.clock_rate_hz);
  return ticks / clock_rate * kMicrosPerSecond +
         ticks % clock_rate * kMicrosPerSecond / clock_rate;
}

}  // namespace ave::media::rtp_session
