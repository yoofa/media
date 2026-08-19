/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_SESSION_RTP_TIMESTAMP_MAPPER_H_
#define MEDIA_MODULES_RTP_SESSION_RTP_TIMESTAMP_MAPPER_H_

#include <cstdint>
#include <optional>

#include "base/numerics/sequence_number_unwrapper.h"
#include "base/units/timestamp.h"

namespace ave::media::rtp_session {

// Maps media PTS values to an RTP clock while preserving a caller-provided
// randomized timestamp origin.
class RtpTimestampMapper {
 public:
  RtpTimestampMapper(uint32_t clock_rate_hz, uint32_t initial_timestamp);

  uint32_t Map(base::Timestamp pts);
  void Reset(uint32_t initial_timestamp);

 private:
  int64_t ToClockTicks(base::TimeDelta duration) const;

  uint32_t clock_rate_hz_;
  uint32_t initial_timestamp_;
  std::optional<base::Timestamp> first_pts_;
};

// Converts a remote RTP clock to a monotonic local media timeline. Sender
// restarts and large timestamp jumps begin a new epoch just after the last
// delivered timestamp instead of producing a negative or multi-hour PTS.
class RtpReceiveTimestampMapper {
 public:
  struct Config {
    uint32_t clock_rate_hz = 0;
    uint32_t maximum_continuous_gap_ms = 5000;
    uint32_t restart_gap_us = 1000;
  };

  explicit RtpReceiveTimestampMapper(Config config);

  base::Timestamp Map(uint32_t rtp_timestamp);
  void Reset();

 private:
  int64_t ToMicros(int64_t ticks) const;

  const Config config_;
  base::RtpTimestampUnwrapper timestamp_unwrapper_;
  std::optional<int64_t> first_rtp_timestamp_;
  std::optional<int64_t> last_rtp_timestamp_;
  int64_t pts_epoch_us_ = 0;
  int64_t last_pts_us_ = -1;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_RTP_TIMESTAMP_MAPPER_H_
