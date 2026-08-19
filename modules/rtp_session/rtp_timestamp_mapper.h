/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_SESSION_RTP_TIMESTAMP_MAPPER_H_
#define MEDIA_MODULES_RTP_SESSION_RTP_TIMESTAMP_MAPPER_H_

#include <cstdint>
#include <optional>

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

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_RTP_TIMESTAMP_MAPPER_H_
