/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_INTERPOLATOR_H_
#define AVE_MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_INTERPOLATOR_H_

#include <cstdint>
#include <optional>

#include <span>
#include "base/clock.h"
#include "base/mutex.h"
#include "base/thread_annotation.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_headers.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace base = ::ave::base;
using std::span;
using Mutex = ::ave::base::Mutex;

//
// Helper class for interpolating the `AbsoluteCaptureTime` header extension.
//
// Supports the "timestamp interpolation" optimization:
//   A receiver SHOULD memorize the capture system (i.e. CSRC/SSRC), capture
//   timestamp, and RTP timestamp of the most recently received abs-capture-time
//   packet on each received stream. It can then use that information, in
//   combination with RTP timestamps of packets without abs-capture-time, to
//   extrapolate missing capture timestamps.
//
// See: https://webrtc.org/experiments/rtp-hdrext/abs-capture-time/
//
class AbsoluteCaptureTimeInterpolator {
 public:
  static constexpr base::TimeDelta kInterpolationMaxInterval =
      base::TimeDelta::Seconds(5);

  explicit AbsoluteCaptureTimeInterpolator(base::Clock* clock);

  // Returns the source (i.e. SSRC or CSRC) of the capture system.
  static uint32_t GetSource(uint32_t ssrc, std::span<const uint32_t> csrcs);

  // Returns a received header extension, an interpolated header extension, or
  // `std::nullopt` if it's not possible to interpolate a header extension.
  std::optional<AbsoluteCaptureTime> OnReceivePacket(
      uint32_t source,
      uint32_t rtp_timestamp,
      int32_t rtp_clock_frequency_hz,
      const std::optional<AbsoluteCaptureTime>& received_extension);

 private:
  friend class AbsoluteCaptureTimeSender;

  static uint64_t InterpolateAbsoluteCaptureTimestamp(
      uint32_t rtp_timestamp,
      int32_t rtp_clock_frequency_hz,
      uint32_t last_rtp_timestamp,
      uint64_t last_absolute_capture_timestamp);

  bool ShouldInterpolateExtension(base::Timestamp receive_time,
                                  uint32_t source,
                                  uint32_t rtp_timestamp,
                                  int32_t rtp_clock_frequency_hz) const
      REQUIRES(mutex_);

  base::Clock* const clock_;

  mutable Mutex mutex_;

  // Time of the last received header extension eligible for interpolation,
  // MinusInfinity() if no extension was received, or last received one is
  // not eligible for interpolation.
  base::Timestamp last_receive_time_ GUARDED_BY(mutex_) =
      base::Timestamp::MinusInfinity();

  uint32_t last_source_ GUARDED_BY(mutex_) = 0;
  uint32_t last_rtp_timestamp_ GUARDED_BY(mutex_) = 0;
  int32_t last_rtp_clock_frequency_hz_ GUARDED_BY(mutex_) = 0;
  AbsoluteCaptureTime last_received_extension_ GUARDED_BY(mutex_);
  // Variables used for statistics generation
  std::optional<base::Timestamp> first_packet_time_;
  std::optional<base::Timestamp> first_offset_time_;
  std::optional<base::Timestamp> first_extension_time_;
  std::optional<base::TimeDelta> previous_capture_delta_;
  std::optional<base::TimeDelta> previous_offset_as_delta_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_INTERPOLATOR_H_
