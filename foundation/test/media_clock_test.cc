/*
 * media_clock_test.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/foundation/media_clock.h"

#include "base/attributes.h"
#include "base/logging.h"
#include "base/time_utils.h"
#include "gtest/gtest.h"

#include <thread>

namespace ave {
namespace media {

// static constexpr int64_t kTestRealDiffUs = 1000000;

namespace {
// Helper class for testing timer events
class TestTimerEvent : public TimerEvent {
 public:
  explicit TestTimerEvent(std::function<void(TimerReason)> callback)
      : callback_(std::move(callback)) {}
  ~TestTimerEvent() override = default;

  void OnTimerEvent(TimerReason reason AVE_MAYBE_UNUSED) override {
    callback_(reason);
  }

 private:
  std::function<void(TimerReason)> callback_;
};

// Helper class for testing clock callbacks
class TestClockCallback : public MediaClock::Callback {
 public:
  ~TestClockCallback() override = default;
  void OnDiscontinuity(int64_t anchor_media_us,
                       int64_t anchor_real_us,
                       float playback_rate) override {
    last_anchor_media_us_ = anchor_media_us;
    last_anchor_real_us_ = anchor_real_us;
    last_playback_rate_ = playback_rate;
    callback_count_++;
  }

  int64_t last_anchor_media_us() const { return last_anchor_media_us_; }
  int64_t last_anchor_real_us() const { return last_anchor_real_us_; }
  float last_playback_rate() const { return last_playback_rate_; }
  int callback_count() const { return callback_count_; }

 private:
  int64_t last_anchor_media_us_ = -1;
  int64_t last_anchor_real_us_ = -1;
  float last_playback_rate_ = 1.0f;
  int callback_count_ = 0;
};
}  // namespace

class MediaClockTest : public ::testing::Test {
 protected:
  void SetUp() override { media_clock_ = std::make_shared<MediaClock>(); }

  std::shared_ptr<MediaClock> media_clock_;
};

// Test initial state
TEST_F(MediaClockTest, InitialState) {
  EXPECT_FLOAT_EQ(1.0f, media_clock_->GetPlaybackRate());

  int64_t media_time = 0;
  EXPECT_EQ(NO_INIT, media_clock_->GetMediaTime(0, &media_time));
}

// Test playback rate control
TEST_F(MediaClockTest, PlaybackRateControl) {
  // Test normal rate setting
  media_clock_->SetPlaybackRate(2.0f);
  EXPECT_FLOAT_EQ(2.0f, media_clock_->GetPlaybackRate());

  // Test pause (rate = 0)
  media_clock_->SetPlaybackRate(0.0f);
  EXPECT_FLOAT_EQ(0.0f, media_clock_->GetPlaybackRate());

  // Test negative rate (should be rejected)
  EXPECT_DEATH(media_clock_->SetPlaybackRate(-1.0f), "");
}

// Test anchor time updates
TEST_F(MediaClockTest, AnchorTimeUpdate) {
  // Basic anchor setting
  media_clock_->UpdateAnchor(1000000);
  int64_t media_time = 0;

  // Test with actual time difference
  int64_t real_time_later = base::TimeMicros();
  EXPECT_EQ(OK, media_clock_->GetMediaTime(real_time_later, &media_time));
  EXPECT_GE(media_time,
            1000000);  // Should be at least equal to the set anchor time

  // Test time progression
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  real_time_later = base::TimeMicros();
  EXPECT_EQ(OK, media_clock_->GetMediaTime(real_time_later, &media_time));
  EXPECT_GE(media_time, 1000000);  // Time should continue to advance

  // Test anchor clearing
  media_clock_->ClearAnchor();
  EXPECT_EQ(NO_INIT,
            media_clock_->GetMediaTime(base::TimeMicros(), &media_time));
}

// Test starting time media
TEST_F(MediaClockTest, StartingTimeMedia) {
  media_clock_->SetStartingTimeMedia(500000);
  media_clock_->UpdateAnchor(1000000, 2000000);

  int64_t media_time = 0;
  EXPECT_EQ(OK, media_clock_->GetMediaTime(1500000, &media_time));
  EXPECT_EQ(500000, media_time);

  // Test time cannot be less than starting time
  EXPECT_EQ(OK, media_clock_->GetMediaTime(1000000, &media_time));
  EXPECT_EQ(500000, media_time);
}

// Test maximum media time
TEST_F(MediaClockTest, MaxTimeMedia) {
  media_clock_->UpdateAnchor(1000000, 3000000);

  int64_t media_time = 0;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Time within normal range
  EXPECT_EQ(OK, media_clock_->GetMediaTime(base::TimeMicros(), &media_time));
  EXPECT_GE(media_time, 1000000);
  EXPECT_LE(media_time, 3000000);

  // Exceeding maximum time
  std::this_thread::sleep_for(std::chrono::seconds(3));
  EXPECT_EQ(OK,
            media_clock_->GetMediaTime(base::TimeMicros(), &media_time, false));
  EXPECT_EQ(3000000, media_time);

  // Allow exceeding maximum time
  EXPECT_EQ(OK,
            media_clock_->GetMediaTime(base::TimeMicros(), &media_time, true));
  EXPECT_GT(media_time, 3000000);
}

// Test real time conversion
TEST_F(MediaClockTest, RealTimeConversion) {
  media_clock_->UpdateAnchor(1000000);
  int64_t start_real_us = base::TimeMicros();

  int64_t real_time = 0;

  // 基本转换 (1x速度)
  media_clock_->SetPlaybackRate(1.0f);
  EXPECT_EQ(OK, media_clock_->GetRealTimeFor(2000000, &real_time));
  EXPECT_GT(real_time, start_real_us);

  // 0.5x速度
  media_clock_->SetPlaybackRate(0.5f);
  EXPECT_EQ(OK, media_clock_->GetRealTimeFor(3000000, &real_time));
  EXPECT_GT(real_time, start_real_us);

  // 2x速度
  media_clock_->SetPlaybackRate(2.0f);
  EXPECT_EQ(OK, media_clock_->GetRealTimeFor(3000000, &real_time));
  EXPECT_GT(real_time, start_real_us);
}

// Test timer events
TEST_F(MediaClockTest, TimerEvents) {
  media_clock_->UpdateAnchor(1000000);

  int timer_count = 0;
  TimerReason last_reason = TimerReason::TIMER_REASON_REACHED;

  // 添加定时器
  media_clock_->AddTimerEvent(
      [&](TimerReason reason AVE_MAYBE_UNUSED) {
        timer_count++;
        last_reason = reason;
      },
      1050000);

  // 等待定时器触发
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  EXPECT_EQ(1, timer_count);
  EXPECT_EQ(TimerReason::TIMER_REASON_REACHED, last_reason);

  // 测试重置时的定时器行为
  timer_count = 0;
  media_clock_->AddTimerEvent(std::make_unique<TestTimerEvent>(
                                  [&](TimerReason reason AVE_MAYBE_UNUSED) {
                                    timer_count++;
                                    last_reason = reason;
                                  }),
                              2500000);

  media_clock_->ClearAnchor();
  EXPECT_EQ(1, timer_count);
  EXPECT_EQ(TimerReason::TIMER_REASON_RESET, last_reason);
}

// Test callback notification
TEST_F(MediaClockTest, CallbackNotification) {
  auto callback = std::make_shared<TestClockCallback>();
  media_clock_->SetNotificationCallback(callback.get());

  // 测试锚点更新触发回调
  media_clock_->UpdateAnchor(1000000);
  EXPECT_EQ(1, callback->callback_count());
  EXPECT_EQ(1000000, callback->last_anchor_media_us());
  EXPECT_NE(-1, callback->last_anchor_real_us());
  EXPECT_FLOAT_EQ(1.0f, callback->last_playback_rate());

  // 测试播放速率变化触发回调
  media_clock_->SetPlaybackRate(2.0f);
  EXPECT_EQ(2, callback->callback_count());
  EXPECT_FLOAT_EQ(2.0f, callback->last_playback_rate());

  // 测试清除锚点触发回调
  media_clock_->ClearAnchor();
  EXPECT_EQ(3, callback->callback_count());
  EXPECT_EQ(-1, callback->last_anchor_media_us());
  EXPECT_EQ(-1, callback->last_anchor_real_us());
}

// Test edge cases
TEST_F(MediaClockTest, EdgeCases) {
  // Test invalid parameters
  EXPECT_EQ(BAD_VALUE, media_clock_->GetMediaTime(0, nullptr));
  EXPECT_EQ(BAD_VALUE, media_clock_->GetRealTimeFor(0, nullptr));

  // Test limit values
  media_clock_->UpdateAnchor(INT64_MAX - 1000000, INT64_MAX - 2000000);
  int64_t media_time = 0;
  EXPECT_EQ(OK, media_clock_->GetMediaTime(INT64_MAX - 1500000, &media_time));

  // Test negative values
  media_clock_->UpdateAnchor(-1000000, 0);
  EXPECT_EQ(OK, media_clock_->GetMediaTime(1000000, &media_time));
}

// Test AddTimerEvent with callback function
TEST_F(MediaClockTest, TimerEventWithCallback) {
  media_clock_->UpdateAnchor(1000000);

  int timer_count = 0;
  // Test adding timer with callback function
  media_clock_->AddTimerEvent(
      [&](TimerReason reason AVE_MAYBE_UNUSED) { timer_count++; },
      1500000,  // media_time_us
      100000);  // adjust_real_us with non-zero value

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  EXPECT_EQ(1, timer_count);
}

// Test multiple timer events
TEST_F(MediaClockTest, MultipleTimerEvents) {
  media_clock_->UpdateAnchor(1000000);

  std::vector<int> triggered_timers;
  for (int i = 0; i < 3; i++) {
    media_clock_->AddTimerEvent(
        [&triggered_timers, i](TimerReason reason AVE_MAYBE_UNUSED) {
          triggered_timers.push_back(i);
        },
        1100000 + i * 100000);  // Staggered timer events
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  EXPECT_EQ(3, static_cast<int>(triggered_timers.size()));
  // Verify order of triggered timers
  EXPECT_EQ(0, triggered_timers[0]);
  EXPECT_EQ(1, triggered_timers[1]);
  EXPECT_EQ(2, triggered_timers[2]);
}

// Test timer behavior with different playback rates
TEST_F(MediaClockTest, TimerWithPlaybackRate) {
  media_clock_->UpdateAnchor(1000000);

  int timer_count = 0;
  // Add timer that should trigger after 500ms at normal speed
  media_clock_->AddTimerEvent(
      [&](TimerReason reason AVE_MAYBE_UNUSED) { timer_count++; }, 1500000);

  // Set 2x playback speed
  media_clock_->SetPlaybackRate(2.0f);

  // Should trigger after ~250ms real time due to 2x speed
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(1, timer_count);
}

// Test concurrent anchor updates and timer events
TEST_F(MediaClockTest, ConcurrentAnchorsAndTimers) {
  media_clock_->UpdateAnchor(1000000);

  int timer_count = 0;
  media_clock_->AddTimerEvent(
      [&](TimerReason reason AVE_MAYBE_UNUSED) { timer_count++; }, 2000000);

  // Update anchor while timer is pending
  media_clock_->UpdateAnchor(1500000);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  EXPECT_EQ(1, timer_count);
}

// Test notification callback with multiple discontinuities
TEST_F(MediaClockTest, MultipleDiscontinuities) {
  auto callback = std::make_shared<TestClockCallback>();
  media_clock_->SetNotificationCallback(callback.get());

  // Test multiple anchor updates in succession
  media_clock_->UpdateAnchor(1000000);
  media_clock_->UpdateAnchor(2000000);
  media_clock_->UpdateAnchor(3000000);

  EXPECT_EQ(3, callback->callback_count());
  EXPECT_EQ(3000000, callback->last_anchor_media_us());
}

// Test timer behavior during pause and resume
TEST_F(MediaClockTest, TimerDuringPauseResume) {
  media_clock_->UpdateAnchor(1000000);

  int timer_count = 0;
  media_clock_->AddTimerEvent(
      [&](TimerReason reason AVE_MAYBE_UNUSED) { timer_count++; }, 2000000);

  // Pause playback
  media_clock_->SetPlaybackRate(0.0f);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  EXPECT_EQ(0, timer_count);  // Timer shouldn't trigger while paused

  // Resume playback
  media_clock_->SetPlaybackRate(1.0f);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  EXPECT_EQ(1, timer_count);  // Timer should trigger after resume
}

// Test behavior with invalid timer events
TEST_F(MediaClockTest, InvalidTimerEvents) {
  // Add timer without setting anchor first
  int timer_count = 0;
  media_clock_->AddTimerEvent(
      [&](TimerReason reason AVE_MAYBE_UNUSED) { timer_count++; }, 1000000);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(0, timer_count);  // Timer shouldn't trigger without anchor

  // Add timer with negative media time
  media_clock_->UpdateAnchor(1000000);
  media_clock_->AddTimerEvent(
      [&](TimerReason reason AVE_MAYBE_UNUSED) { timer_count++; }, -1000000);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(0, timer_count);  // Timer shouldn't trigger with negative time
}

}  // namespace media
}  // namespace ave
