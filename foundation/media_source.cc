/*
 * media_source.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_source.h"

namespace ave {
namespace media {

MediaSource::ReadOptions::ReadOptions()
    : options_(0),
      seek_time_us_(0),
      seek_mode_(SEEK_CLOSEST_SYNC),
      lateness_us_(0),
      non_blocking_(false) {
  Reset();
}

void MediaSource::ReadOptions::Reset() {
  options_ = 0;
  seek_time_us_ = 0;
  seek_mode_ = SEEK_CLOSEST_SYNC;
  lateness_us_ = 0;
  non_blocking_ = false;
}

void MediaSource::ReadOptions::SetSeekTo(int64_t time_us, SeekMode mode) {
  seek_time_us_ = time_us;
  seek_mode_ = mode;
  options_ |= kSeekTo_Option;
}

void MediaSource::ReadOptions::ClearSeekTo() {
  options_ &= ~kSeekTo_Option;
}

bool MediaSource::ReadOptions::GetSeekTo(int64_t* time_us,
                                         SeekMode* mode) const {
  if (options_ & kSeekTo_Option) {
    *time_us = seek_time_us_;
    *mode = seek_mode_;
    return true;
  }
  return false;
}

void MediaSource::ReadOptions::SetLateBy(int64_t lateness_us) {
  lateness_us_ = lateness_us;
}

int64_t MediaSource::ReadOptions::GetLateBy() const {
  return lateness_us_;
}

void MediaSource::ReadOptions::SetNonBlocking() {
  non_blocking_ = true;
}

void MediaSource::ReadOptions::ClearNonBlocking() {
  non_blocking_ = false;
}

bool MediaSource::ReadOptions::GetNonBlocking() const {
  return non_blocking_;
}

}  // namespace media
}  // namespace ave
