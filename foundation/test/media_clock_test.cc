/*
 * media_clock_test.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/foundation/media_clock.h"

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

  void OnTimerEvent(TimerReason reason) override { callback_(reason); }

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

// 测试初始状态
TEST_F(MediaClockTest, InitialState) {
  EXPECT_FLOAT_EQ(1.0f, media_clock_->GetPlaybackRate());

  int64_t media_time = 0;
  EXPECT_EQ(NO_INIT, media_clock_->GetMediaTime(0, &media_time));
}

// 测试播放速率设置
TEST_F(MediaClockTest, PlaybackRateControl) {
  // 测试正常速率设置
  media_clock_->SetPlaybackRate(2.0f);
  EXPECT_FLOAT_EQ(2.0f, media_clock_->GetPlaybackRate());

  // 测试暂停(速率为0)
  media_clock_->SetPlaybackRate(0.0f);
  EXPECT_FLOAT_EQ(0.0f, media_clock_->GetPlaybackRate());

  // 测试负速率(应该被拒绝)
  EXPECT_DEATH(media_clock_->SetPlaybackRate(-1.0f), "");
}

// 测试锚点更新
TEST_F(MediaClockTest, AnchorTimeUpdate) {
  // 基本锚点设置
  media_clock_->UpdateAnchor(1000000);
  int64_t media_time = 0;

  // 使用实际的时间差来测试
  int64_t real_time_later = base::TimeMicros();
  EXPECT_EQ(OK, media_clock_->GetMediaTime(real_time_later, &media_time));
  EXPECT_GE(media_time, 1000000);  // 应该至少等于设置的锚点时间

  // 测试时间推进
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  real_time_later = base::TimeMicros();
  EXPECT_EQ(OK, media_clock_->GetMediaTime(real_time_later, &media_time));
  EXPECT_GE(media_time, 1000000);  // 时间应该继续前进

  // 测试清除锚点
  media_clock_->ClearAnchor();
  EXPECT_EQ(NO_INIT,
            media_clock_->GetMediaTime(base::TimeMicros(), &media_time));
}

// 测试起始时间设置
TEST_F(MediaClockTest, StartingTimeMedia) {
  media_clock_->SetStartingTimeMedia(500000);
  media_clock_->UpdateAnchor(1000000, 2000000);

  int64_t media_time = 0;
  EXPECT_EQ(OK, media_clock_->GetMediaTime(1500000, &media_time));
  EXPECT_EQ(500000, media_time);

  // 测试时间不能小于起始时间
  EXPECT_EQ(OK, media_clock_->GetMediaTime(1000000, &media_time));
  EXPECT_EQ(500000, media_time);
}

// 测试最大媒体时间
TEST_F(MediaClockTest, MaxTimeMedia) {
  media_clock_->UpdateAnchor(1000000, 3000000);

  int64_t media_time = 0;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // 正常范围内的时间
  EXPECT_EQ(OK, media_clock_->GetMediaTime(base::TimeMicros(), &media_time));
  EXPECT_GE(media_time, 1000000);
  EXPECT_LE(media_time, 3000000);

  // 超过最大时间
  std::this_thread::sleep_for(std::chrono::seconds(3));
  EXPECT_EQ(OK,
            media_clock_->GetMediaTime(base::TimeMicros(), &media_time, false));
  EXPECT_EQ(3000000, media_time);

  // 允许超过最大时间
  EXPECT_EQ(OK,
            media_clock_->GetMediaTime(base::TimeMicros(), &media_time, true));
  EXPECT_GT(media_time, 3000000);
}

// 测试实时时间转换
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

// 测试定时器事件
TEST_F(MediaClockTest, TimerEvents) {
  media_clock_->UpdateAnchor(1000000);

  int timer_count = 0;
  TimerReason last_reason = TimerReason::TIMER_REASON_REACHED;

  // 添加定时器
  media_clock_->AddTimerEvent(
      [&](TimerReason reason) {
        AVE_LOG(LS_INFO) << "TimerEvent, reason: " << reason;
        timer_count++;
        last_reason = reason;
      },
      1050000);

  // 等待定时器触发
  AVE_LOG(LS_INFO) << "Sleep 1000ms";
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  AVE_LOG(LS_INFO) << "Sleep 1000ms done";

  EXPECT_EQ(1, timer_count);
  EXPECT_EQ(TimerReason::TIMER_REASON_REACHED, last_reason);

  // 测试重置时的定时器行为
  timer_count = 0;
  media_clock_->AddTimerEvent(
      std::make_unique<TestTimerEvent>([&](TimerReason reason) {
        timer_count++;
        last_reason = reason;
      }),
      2500000);

  media_clock_->ClearAnchor();
  EXPECT_EQ(1, timer_count);
  EXPECT_EQ(TimerReason::TIMER_REASON_RESET, last_reason);
}

// 测试回调通知
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

// 测试边界条件
TEST_F(MediaClockTest, EdgeCases) {
  // 测试无效参数
  EXPECT_DEATH(media_clock_->GetMediaTime(0, nullptr), "");
  EXPECT_DEATH(media_clock_->GetRealTimeFor(0, nullptr), "");

  // 测试极限值
  media_clock_->UpdateAnchor(INT64_MAX - 1000000, INT64_MAX - 2000000);
  int64_t media_time = 0;
  EXPECT_EQ(OK, media_clock_->GetMediaTime(INT64_MAX - 1500000, &media_time));

  // 测试负值
  media_clock_->UpdateAnchor(-1000000, 0);
  EXPECT_EQ(OK, media_clock_->GetMediaTime(1000000, &media_time));
  EXPECT_EQ(0, media_time);  // 应该被限制在0
}

}  // namespace media
}  // namespace ave
