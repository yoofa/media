/*
 * media_clock.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_CLOCK_H
#define MEDIA_CLOCK_H

#include <functional>
#include <list>
#include <mutex>
#include <type_traits>

#include "base/errors.h"
#include "base/task_util/task_runner.h"
#include "base/thread_annotation.h"

namespace ave {
namespace media {

using ave::base::TaskRunner;

enum TimerReason {
  TIMER_REASON_REACHED = 0,
  TIMER_REASON_RESET = 1,
};

struct TimerEvent {
  virtual ~TimerEvent() = default;
  virtual void OnTimerEvent(TimerReason reason) = 0;
};

namespace clock_impl {

template <typename Closure>
class ClosureEvent : public TimerEvent {
 public:
  explicit ClosureEvent(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}

 private:
  void OnTimerEvent(TimerReason reason) override { closure_(reason); }
  std::decay_t<Closure> closure_;
};

template <typename Closure>
std::unique_ptr<TimerEvent> ToTimerEvent(Closure&& closure) {
  return std::make_unique<clock_impl::ClosureEvent<Closure>>(
      std::forward<Closure>(closure));
}

}  // namespace clock_impl

class MediaClock {
 public:
  struct Callback {
    virtual ~Callback() = default;
    virtual void OnDiscontinuity(int64_t anchor_media_us,
                                 int64_t anchor_real_us,
                                 float playback_rate) = 0;
  };

  explicit MediaClock();
  ~MediaClock();

  void SetStartingTimeMedia(int64_t starting_time_media_us);
  void ClearAnchor();
  void UpdateAnchor(int64_t anchor_time_media_us,
                    int64_t max_time_media_us = INT64_MAX);
  void UpdateMaxTimeMedia(int64_t max_time_media_us);
  void SetPlaybackRate(float rate);
  float GetPlaybackRate() const;
  status_t GetMediaTime(int64_t real_us,
                        int64_t* out_media_us,
                        bool allow_past_max_time = false) const;
  status_t GetRealTimeFor(int64_t target_media_us, int64_t* out_real_us) const;
  
  void AddTimerEvent(std::unique_ptr<TimerEvent> event,
                     int64_t media_time_us,
                     int64_t adjust_real_us = 0);

  template <class Closure,
            std::enable_if_t<
                !std::is_convertible_v<Closure, std::unique_ptr<TimerEvent>>>* =
                nullptr>
  void AddTimerEvent(Closure&& closure,
                     int64_t media_time_us,
                     int64_t adjust_real_us = 0) {
    AddTimerEvent(clock_impl::ToTimerEvent(closure), media_time_us,
                  adjust_real_us);
  }

  void SetNotificationCallback(Callback* callback);

 private:
  struct Timer {
    Timer(std::unique_ptr<TimerEvent> callback,
          int64_t media_time_us,
          int64_t adjust_real_us)
        : callback(std::move(callback)),
          media_time_us(media_time_us),
          adjust_real_us(adjust_real_us) {}

    std::unique_ptr<TimerEvent> callback = nullptr;
    int64_t media_time_us = 0;
    int64_t adjust_real_us = 0;
  };

  status_t GetMediaTime_l(int64_t real_us,
                          int64_t* out_media_us,
                          bool allow_past_max_time) const REQUIRES(mutex_);

  void Reset();

  void ProcessTimers() REQUIRES(mutex_);
  void PostProcessTimers(int64_t delay_us = 0);
  void UpdateAnchorTimesAndPlaybackRate(int64_t anchor_time_media_us,
                                        int64_t anchor_time_real_us,
                                        float playback_rate) REQUIRES(mutex_);
  void NotifyDiscontinuity() REQUIRES(mutex_);

  std::unique_ptr<TaskRunner> task_runner_;
  mutable std::mutex mutex_;
  int64_t starting_time_media_us_ GUARDED_BY(mutex_);
  int64_t anchor_time_media_us_ GUARDED_BY(mutex_);
  int64_t anchor_time_real_us_ GUARDED_BY(mutex_);
  int64_t max_time_media_us_ GUARDED_BY(mutex_);
  float playback_rate_ GUARDED_BY(mutex_);
  Callback* notify_callback_;
  std::list<Timer> timers_ GUARDED_BY(mutex_);
};

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_CLOCK_H */
