/*
 * media_clock_test.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/foundation/media_clock.h"

#include "gtest/gtest.h"

namespace ave {
namespace media {

class MediaClockTest : public ::testing::Test {
 protected:
  void SetUp() override { media_clock_ = std::make_shared<MediaClock>(); }

  std::shared_ptr<MediaClock> media_clock_;
};

TEST_F(MediaClockTest, InitialState) {
  EXPECT_FLOAT_EQ(1.0f, media_clock_->GetPlaybackRate());

  int64_t media_time = 0;
  EXPECT_EQ(NO_INIT, media_clock_->GetMediaTime(0, &media_time));
}

TEST_F(MediaClockTest, SetPlaybackRate) {
  media_clock_->SetPlaybackRate(2.0f);
  EXPECT_FLOAT_EQ(2.0f, media_clock_->GetPlaybackRate());
}

TEST_F(MediaClockTest, UpdateAnchor) {
  media_clock_->UpdateAnchor(1000000, 2000000);

  int64_t media_time = 0;
  EXPECT_EQ(OK, media_clock_->GetMediaTime(2000000, &media_time));
  EXPECT_EQ(1000000, media_time);

  EXPECT_EQ(OK, media_clock_->GetMediaTime(3000000, &media_time));
  EXPECT_EQ(2000000, media_time);
}

TEST_F(MediaClockTest, GetRealTimeFor) {
  media_clock_->UpdateAnchor(1000000, 2000000);

  int64_t real_time = 0;
  EXPECT_EQ(OK, media_clock_->GetRealTimeFor(2000000, &real_time));
  EXPECT_EQ(3000000, real_time);
}

TEST_F(MediaClockTest, SetStartingTimeMedia) {
  media_clock_->SetStartingTimeMedia(500000);
  media_clock_->UpdateAnchor(1000000, 2000000);

  int64_t media_time = 0;
  EXPECT_EQ(OK, media_clock_->GetMediaTime(1500000, &media_time));
  EXPECT_EQ(500000, media_time);
}

// Add more tests here for other methods and edge cases

}  // namespace media
}  // namespace ave
