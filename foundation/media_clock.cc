/*
 * media_clock.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_clock.h"

#include <cmath>

#include "base/checks.h"
#include "base/logging.h"
#include "base/task_util/default_task_runner_factory.h"
#include "base/task_util/task_runner_factory.h"
#include "base/time_utils.h"

namespace ave {
namespace media {
using ave::base::CreateDefaultTaskRunnerFactory;
using ave::base::TaskRunnerFactory;

namespace {

// Maximum allowed time backwards from anchor change.
// If larger than this threshold, it's treated as discontinuity.
const int64_t kAnchorFluctuationAllowedUs = 10000LL;

}  // namespace

MediaClock::MediaClock()
    : task_runner_(std::make_unique<TaskRunner>(
          CreateDefaultTaskRunnerFactory()->CreateTaskRunner(
              "MediaClock",
              TaskRunnerFactory::Priority::NORMAL))),
      starting_time_media_us_(-1),
      anchor_time_media_us_(-1),
      anchor_time_real_us_(-1),
      max_time_media_us_(INT64_MAX),
      playback_rate_(1.0),
      notify_callback_(nullptr) {}

MediaClock::~MediaClock() {
  Reset();
  // wait for all timers to be processed
}

void MediaClock::SetStartingTimeMedia(int64_t starting_time_media_us) {
  std::lock_guard<std::mutex> lock(mutex_);
  starting_time_media_us_ = starting_time_media_us;
}

void MediaClock::ClearAnchor() {
  std::lock_guard<std::mutex> lock(mutex_);
  UpdateAnchorTimesAndPlaybackRate(-1, -1, playback_rate_);
}

void MediaClock::UpdateAnchor(int64_t anchor_time_media_us,
                              int64_t anchor_time_real_us,
                              int64_t max_time_media_us) {
  if (anchor_time_media_us < 0 || anchor_time_real_us < 0) {
    AVE_LOG(LS_WARNING) << "Invalid anchor time: " << anchor_time_media_us
                        << ", " << anchor_time_real_us;
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t now_us = base::TimeMicros();
  auto real_diff = static_cast<double>(now_us - anchor_time_real_us);
  auto media_diff = std::llround(real_diff * playback_rate_);
  auto now_media_us = anchor_time_media_us + media_diff;
  if (now_media_us < 0) {
    AVE_LOG(LS_WARNING) << "Reject negative media time: " << now_media_us;
    return;
  }

  if (max_time_media_us != -1) {
    max_time_media_us_ = max_time_media_us;
  }

  if (anchor_time_real_us_ != -1) {
    auto old_real_diff = static_cast<double>(now_us - anchor_time_real_us_);
    auto old_media_diff = std::llround(old_real_diff * playback_rate_);
    auto old_media_us = anchor_time_media_us_ + old_media_diff;
    if (std::abs(now_media_us - old_media_us) < kAnchorFluctuationAllowedUs) {
      AVE_LOG(LS_VERBOSE) << " too closet, no need to update anchor, "
                          << "now_media_us: " << now_media_us
                          << ", old_media_us: " << old_media_us;
      return;
    }
  }
  UpdateAnchorTimesAndPlaybackRate(now_media_us, now_us, playback_rate_);
  PostProcessTimers();
}

void MediaClock::UpdateMaxTimeMedia(int64_t max_time_media_us) {
  std::lock_guard<std::mutex> lock(mutex_);
  max_time_media_us_ = max_time_media_us;
}

void MediaClock::SetPlaybackRate(float rate) {
  AVE_CHECK_GT(rate, 0.0);
  std::lock_guard<std::mutex> lock(mutex_);
  if (anchor_time_media_us_ == -1) {
    playback_rate_ = rate;
    return;
  }

  int64_t now_us = base::TimeMicros();
  int64_t now_media_us =
      anchor_time_media_us_ +
      std::llround(static_cast<double>(now_us - anchor_time_real_us_) *
                   playback_rate_);

  if (now_media_us < 0) {
    AVE_LOG(LS_WARNING) << "SetPlaybackRate, negative media time set to 0";
    now_media_us = 0;
    return;
  }

  UpdateAnchorTimesAndPlaybackRate(now_media_us, now_us, rate);

  if (rate > 0.0) {
    PostProcessTimers();
  }
}

float MediaClock::GetPlaybackRate() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return playback_rate_;
}

status_t MediaClock::GetMediaTime(int64_t real_us,
                                  int64_t* out_media_us,
                                  bool allow_past_max_time) const {
  if (out_media_us == nullptr) {
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  return GetMediaTime_l(real_us, out_media_us, allow_past_max_time);
}

status_t MediaClock::GetRealTimeFor(int64_t target_media_us,
                                    int64_t* out_real_us) const {
  if (out_real_us == nullptr) {
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (playback_rate_ == 0.0) {
    return NO_INIT;
  }

  auto now_us = base::TimeMicros();
  int64_t now_media_us = 0;
  status_t status = GetMediaTime_l(now_us, &now_media_us, true);
  if (status != OK) {
    return status;
  }

  *out_real_us =
      now_us +
      std::llround(static_cast<double>(target_media_us - now_media_us) /
                   playback_rate_);
  return OK;
}

void MediaClock::AddTimerEvent(std::unique_ptr<TimerEvent> event,
                               int64_t media_time_us,
                               int64_t adjust_real_us) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (anchor_time_media_us_ == -1) {
    AVE_LOG(LS_WARNING) << "AddTimerEvent, no anchor time set";
    return;
  }

  bool trigger_now = (playback_rate_ != 0.0);
  // TODO: if is not lastest event, do not trigger now

  timers_.emplace_back(std::move(event), media_time_us, adjust_real_us);
  if (trigger_now) {
    PostProcessTimers();
  }
}

void MediaClock::SetNotificationCallback(Callback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  notify_callback_ = callback;
}

// private
void MediaClock::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  // process all timers and wait for them to be processed
  auto it = timers_.begin();
  while (it != timers_.end()) {
    task_runner_->PostTask([callback = std::move(it->callback)]() {
      callback->OnTimerEvent(TimerReason::TIMER_REASON_RESET);
    });
  }
  max_time_media_us_ = INT64_MAX;
  starting_time_media_us_ = -1;
  UpdateAnchorTimesAndPlaybackRate(-1, -1, playback_rate_);
}

void MediaClock::UpdateAnchorTimesAndPlaybackRate(int64_t anchor_time_media_us,
                                                  int64_t anchor_time_real_us,
                                                  float playback_rate) {
  if (anchor_time_media_us_ != anchor_time_media_us ||
      anchor_time_real_us_ != anchor_time_real_us ||
      playback_rate_ != playback_rate) {
    anchor_time_media_us_ = anchor_time_media_us;
    anchor_time_real_us_ = anchor_time_real_us;
    playback_rate_ = playback_rate;
    NotifyDiscontinuity();
  }
}

status_t MediaClock::GetMediaTime_l(int64_t real_us,
                                    int64_t* out_media_us,
                                    bool allow_past_max_time) const {
  // not playing, no init or paused
  if (anchor_time_real_us_ == -1) {
    return NO_INIT;
  }

  auto real_time_since_anchor_us =
      static_cast<double>(real_us - anchor_time_real_us_);
  int64_t media_time_since_anchor_us =
      std::llround(real_time_since_anchor_us * playback_rate_);
  int64_t media_time_us = media_time_since_anchor_us + anchor_time_media_us_;

  if (media_time_us > max_time_media_us_ && !allow_past_max_time) {
    media_time_us = max_time_media_us_;
  }

  if (media_time_us < starting_time_media_us_) {
    media_time_us = starting_time_media_us_;
  }
  if (media_time_us < 0) {
    media_time_us = 0;
  }

  *out_media_us = media_time_us;
  return OK;
}

void MediaClock::ProcessTimers() {
  int64_t now_us = base::TimeMicros();
  int64_t now_media_us = 0;
  auto status = GetMediaTime_l(now_us, &now_media_us, false);

  if (status != OK) {
    return;
  }

  int64_t delay_us = INT64_MAX;

  auto it = timers_.begin();
  std::list<std::unique_ptr<TimerEvent>> notify_list;

  while (it != timers_.end()) {
    auto diff = static_cast<double>(it->adjust_real_us) * playback_rate_ +
                static_cast<double>(it->media_time_us - now_media_us);
    int64_t diff_media_us = 0;
    if (diff > static_cast<double>(INT64_MAX)) {
      diff_media_us = INT64_MAX;
    } else if (diff < static_cast<double>(INT64_MIN)) {
      diff_media_us = INT64_MIN;
    } else {
      diff_media_us = static_cast<int64_t>(diff);
    }
    if (diff_media_us <= 0) {
      notify_list.push_back(std::move(it->callback));
      it = timers_.erase(it);
    } else {
      if (playback_rate_ != 0.0 &&
          diff_media_us <
              static_cast<int64_t>(std::floor(static_cast<double>(INT64_MAX) *
                                              playback_rate_))) {
        int64_t target_delay_us =
            std::llround(static_cast<double>(diff_media_us) / playback_rate_);
        delay_us = std::min(delay_us, target_delay_us);
      }
      ++it;
    }
  }

  auto it_notify = notify_list.begin();
  while (it_notify != notify_list.end()) {
    // TODO: post in task runner maybe better?
    (*it_notify)->OnTimerEvent(TimerReason::TIMER_REASON_REACHED);
    it_notify = notify_list.erase(it_notify);
  }

  // while (it != timers_.end()) {
  //   if (it->media_time_us <= now_media_us) {
  //     task_runner_->PostTask([callback = std::move(it->callback)](int64_t)
  //     {
  //       callback(TimerReason::TIMER_REASON_REACHED);
  //     });
  //     it = timers_.erase(it);
  //   } else {
  //     // calculate delay_us with playback rate
  //     int64_t target_real_us =
  //         anchor_time_real_us_ +
  //         std::llround(
  //             static_cast<double>(it->media_time_us -
  //             anchor_time_media_us_) / playback_rate_);
  //     delay_us =
  //         std::min(delay_us, target_real_us - now_us + it->adjust_real_us);
  //     ++it;
  //   }
  // }

  if (delay_us == INT64_MAX || timers_.empty() || playback_rate_ == 0.0 ||
      anchor_time_media_us_ == -1) {
    return;
  }

  PostProcessTimers(delay_us);
}

void MediaClock::PostProcessTimers(int64_t delay_us) {
  task_runner_->PostDelayedTask(
      [this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        ProcessTimers();
      },
      delay_us);
}

void MediaClock::NotifyDiscontinuity() {
  if (notify_callback_) {
    notify_callback_->OnDiscontinuity(anchor_time_media_us_,
                                      anchor_time_real_us_, playback_rate_);
  }
}
}  // namespace media
}  // namespace ave
