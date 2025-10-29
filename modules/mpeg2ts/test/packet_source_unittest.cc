/*
 * packet_source_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Comprehensive unit tests for PacketSource
 */

#include "../packet_source.h"

#include <gtest/gtest.h>
#include <thread>

#include "foundation/media_defs.h"
#include "foundation/media_frame.h"
#include "foundation/media_meta.h"

namespace ave {
namespace media {
namespace mpeg2ts {

class PacketSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    video_meta_ = MediaMeta::CreatePtr(MediaType::VIDEO,
                                       MediaMeta::FormatType::kTrack);
    video_meta_->SetMime(MEDIA_MIMETYPE_VIDEO_AVC);
    video_meta_->SetWidth(1920);
    video_meta_->SetHeight(1080);

    audio_meta_ = MediaMeta::CreatePtr(MediaType::AUDIO,
                                       MediaMeta::FormatType::kTrack);
    audio_meta_->SetMime(MEDIA_MIMETYPE_AUDIO_AAC);
    audio_meta_->SetSampleRate(44100);
    audio_meta_->SetChannelLayout(CHANNEL_LAYOUT_STEREO);
  }

  std::shared_ptr<MediaMeta> video_meta_;
  std::shared_ptr<MediaMeta> audio_meta_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(PacketSourceTest, ConstructWithVideoMeta) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  EXPECT_NE(source, nullptr);
  
  auto format = source->GetFormat();
  EXPECT_NE(format, nullptr);
  EXPECT_EQ(format->stream_type(), MediaType::VIDEO);
}

TEST_F(PacketSourceTest, ConstructWithAudioMeta) {
  auto source = std::make_shared<PacketSource>(audio_meta_);
  EXPECT_NE(source, nullptr);
  
  auto format = source->GetFormat();
  EXPECT_NE(format, nullptr);
  EXPECT_EQ(format->stream_type(), MediaType::AUDIO);
}

TEST_F(PacketSourceTest, ConstructWithNullMeta) {
  auto source = std::make_shared<PacketSource>(nullptr);
  EXPECT_NE(source, nullptr);
  
  auto format = source->GetFormat();
  EXPECT_EQ(format, nullptr);
}

// ============================================================================
// Start/Stop Tests
// ============================================================================

TEST_F(PacketSourceTest, StartStop) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  EXPECT_EQ(source->Start(nullptr), OK);
  EXPECT_EQ(source->Stop(), OK);
}

// ============================================================================
// Queue/Dequeue Tests
// ============================================================================

TEST_F(PacketSourceTest, QueueAndDequeueFrame) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  frame->SetPts(base::Timestamp::Micros(1000));
  memset(frame->data(), 0xAA, 100);
  
  source->QueueAccessUnit(frame);
  
  std::shared_ptr<MediaFrame> retrieved;
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), OK);
  EXPECT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved->size(), 100u);
  EXPECT_EQ(retrieved->pts().us(), 1000);
}

TEST_F(PacketSourceTest, MultipleFrames) {
  auto source = std::make_shared<PacketSource>(audio_meta_);
  
  for (int i = 0; i < 10; ++i) {
    auto frame = MediaFrame::CreateShared(50, MediaType::AUDIO);
    frame->SetPts(base::Timestamp::Micros(i * 1000));
    source->QueueAccessUnit(frame);
  }
  
  for (int i = 0; i < 10; ++i) {
    std::shared_ptr<MediaFrame> frame;
    EXPECT_EQ(source->DequeueAccessUnit(frame), OK);
    EXPECT_NE(frame, nullptr);
    EXPECT_EQ(frame->pts().us(), i * 1000);
  }
}

TEST_F(PacketSourceTest, DequeueFromEmpty) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  // Signal EOS immediately
  source->SignalEOS(ERROR_END_OF_STREAM);
  
  std::shared_ptr<MediaFrame> frame;
  status_t result = source->DequeueAccessUnit(frame);
  EXPECT_EQ(result, ERROR_END_OF_STREAM);
}

// ============================================================================
// Read Interface Tests
// ============================================================================

TEST_F(PacketSourceTest, ReadInterface) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  auto frame = MediaFrame::CreateShared(200, MediaType::VIDEO);
  frame->SetPts(base::Timestamp::Micros(2000));
  source->QueueAccessUnit(frame);
  
  std::shared_ptr<MediaFrame> read_frame;
  EXPECT_EQ(source->Read(read_frame, nullptr), OK);
  EXPECT_NE(read_frame, nullptr);
  EXPECT_EQ(read_frame->pts().us(), 2000);
}

// ============================================================================
// Discontinuity Tests
// ============================================================================

TEST_F(PacketSourceTest, TimeDiscontinuity) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  auto frame1 = MediaFrame::CreateShared(100, MediaType::VIDEO);
  frame1->SetPts(base::Timestamp::Micros(1000));
  source->QueueAccessUnit(frame1);
  
  source->QueueDiscontinuity(DiscontinuityType::TIME, nullptr, false);
  
  auto frame2 = MediaFrame::CreateShared(100, MediaType::VIDEO);
  frame2->SetPts(base::Timestamp::Micros(5000));
  source->QueueAccessUnit(frame2);
  
  std::shared_ptr<MediaFrame> retrieved;
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), OK);
  EXPECT_EQ(retrieved->pts().us(), 1000);
  
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), INFO_DISCONTINUITY);
  
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), OK);
  EXPECT_EQ(retrieved->pts().us(), 5000);
}

TEST_F(PacketSourceTest, FormatDiscontinuity) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  auto frame1 = MediaFrame::CreateShared(100, MediaType::VIDEO);
  source->QueueAccessUnit(frame1);
  
  source->QueueDiscontinuity(DiscontinuityType::FORMATCHANGE, nullptr, false);
  
  auto frame2 = MediaFrame::CreateShared(100, MediaType::VIDEO);
  source->QueueAccessUnit(frame2);
  
  std::shared_ptr<MediaFrame> retrieved;
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), OK);
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), INFO_DISCONTINUITY);
  
  // Format should be cleared after format discontinuity
  auto format = source->GetFormat();
  EXPECT_EQ(format, nullptr);
}

TEST_F(PacketSourceTest, DiscontinuityWithDiscard) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  // Queue some frames
  for (int i = 0; i < 5; ++i) {
    auto frame = MediaFrame::CreateShared(50, MediaType::VIDEO);
    source->QueueAccessUnit(frame);
  }
  
  // Discontinuity with discard should clear all frames
  source->QueueDiscontinuity(DiscontinuityType::TIME, nullptr, true);
  
  std::shared_ptr<MediaFrame> frame;
  status_t result = source->DequeueAccessUnit(frame);
  // Should get discontinuity marker, not the old frames
  EXPECT_EQ(result, INFO_DISCONTINUITY);
}

// ============================================================================
// EOS Tests
// ============================================================================

TEST_F(PacketSourceTest, SignalEOS) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  source->QueueAccessUnit(frame);
  
  source->SignalEOS(ERROR_END_OF_STREAM);
  
  std::shared_ptr<MediaFrame> retrieved;
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), OK);
  
  EXPECT_EQ(source->DequeueAccessUnit(retrieved), ERROR_END_OF_STREAM);
}

TEST_F(PacketSourceTest, EOSWithoutFrames) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  source->SignalEOS(ERROR_END_OF_STREAM);
  
  std::shared_ptr<MediaFrame> frame;
  EXPECT_EQ(source->DequeueAccessUnit(frame), ERROR_END_OF_STREAM);
}

// ============================================================================
// Buffer Status Tests
// ============================================================================

TEST_F(PacketSourceTest, HasBufferAvailable) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  status_t final_result;
  EXPECT_FALSE(source->HasBufferAvailable(&final_result));
  
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  source->QueueAccessUnit(frame);
  
  EXPECT_TRUE(source->HasBufferAvailable(&final_result));
  EXPECT_EQ(final_result, OK);
}

TEST_F(PacketSourceTest, HasDataBufferAvailable) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  status_t final_result;
  EXPECT_FALSE(source->HasDataBufferAvailable(&final_result));
  
  // Queue discontinuity marker (empty frame)
  source->QueueDiscontinuity(DiscontinuityType::TIME, nullptr, false);
  EXPECT_FALSE(source->HasDataBufferAvailable(&final_result));
  
  // Queue actual data frame
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  source->QueueAccessUnit(frame);
  EXPECT_TRUE(source->HasDataBufferAvailable(&final_result));
}

TEST_F(PacketSourceTest, GetAvailableBufferCount) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  status_t final_result;
  EXPECT_EQ(source->GetAvailableBufferCount(&final_result), 0u);
  
  for (int i = 0; i < 5; ++i) {
    auto frame = MediaFrame::CreateShared(50, MediaType::VIDEO);
    source->QueueAccessUnit(frame);
  }
  
  EXPECT_EQ(source->GetAvailableBufferCount(&final_result), 5u);
}

// ============================================================================
// Buffered Duration Tests
// ============================================================================

TEST_F(PacketSourceTest, GetBufferedDuration) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  // Queue frames with timestamps
  for (int i = 0; i < 5; ++i) {
    auto frame = MediaFrame::CreateShared(50, MediaType::VIDEO);
    frame->SetPts(base::Timestamp::Micros(i * 33333));  // ~30fps
    source->QueueAccessUnit(frame);
  }
  
  status_t final_result;
  int64_t duration = source->GetBufferedDurationUs(&final_result);
  EXPECT_GT(duration, 0);
}

// ============================================================================
// Next Buffer Time Tests
// ============================================================================

TEST_F(PacketSourceTest, NextBufferTime) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  int64_t time_us;
  EXPECT_NE(source->NextBufferTime(&time_us), OK);
  
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  frame->SetPts(base::Timestamp::Micros(12345));
  source->QueueAccessUnit(frame);
  
  EXPECT_EQ(source->NextBufferTime(&time_us), OK);
  EXPECT_EQ(time_us, 12345);
}

// ============================================================================
// IsFinished Tests
// ============================================================================

TEST_F(PacketSourceTest, IsFinishedWithEOS) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  EXPECT_FALSE(source->IsFinished(10000000));
  
  source->SignalEOS(ERROR_END_OF_STREAM);
  EXPECT_TRUE(source->IsFinished(10000000));
}

TEST_F(PacketSourceTest, IsFinishedNearDuration) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  // Queue frame with timestamp near duration
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  frame->SetPts(base::Timestamp::Micros(9999000));  // Within 2s of 10s duration
  source->QueueAccessUnit(frame);
  
  EXPECT_TRUE(source->IsFinished(10000000));
}

// ============================================================================
// Latest Meta Tests
// ============================================================================

TEST_F(PacketSourceTest, LatestEnqueuedMeta) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  EXPECT_EQ(source->GetLatestEnqueuedMeta(), nullptr);
  
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  frame->SetWidth(1920);
  frame->SetHeight(1080);
  source->QueueAccessUnit(frame);
  
  auto meta = source->GetLatestEnqueuedMeta();
  EXPECT_NE(meta, nullptr);
  EXPECT_EQ(meta->width(), 1920);
  EXPECT_EQ(meta->height(), 1080);
}

TEST_F(PacketSourceTest, LatestDequeuedMeta) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  frame->SetWidth(1280);
  frame->SetHeight(720);
  source->QueueAccessUnit(frame);
  
  std::shared_ptr<MediaFrame> retrieved;
  source->DequeueAccessUnit(retrieved);
  
  auto meta = source->GetLatestDequeuedMeta();
  EXPECT_NE(meta, nullptr);
  EXPECT_EQ(meta->width(), 1280);
  EXPECT_EQ(meta->height(), 720);
}

// ============================================================================
// Enable/Disable Tests
// ============================================================================

TEST_F(PacketSourceTest, EnableDisable) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  auto frame = MediaFrame::CreateShared(100, MediaType::VIDEO);
  source->QueueAccessUnit(frame);
  
  // Disable source
  source->Enable(false);
  
  status_t final_result;
  EXPECT_FALSE(source->HasBufferAvailable(&final_result));
  
  // Re-enable
  source->Enable(true);
  EXPECT_TRUE(source->HasBufferAvailable(&final_result));
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F(PacketSourceTest, Clear) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  for (int i = 0; i < 5; ++i) {
    auto frame = MediaFrame::CreateShared(50, MediaType::VIDEO);
    source->QueueAccessUnit(frame);
  }
  
  status_t final_result;
  EXPECT_EQ(source->GetAvailableBufferCount(&final_result), 5u);
  
  source->Clear();
  
  EXPECT_EQ(source->GetAvailableBufferCount(&final_result), 0u);
  EXPECT_EQ(source->GetFormat(), nullptr);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(PacketSourceTest, ConcurrentQueueDequeue) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  std::atomic<bool> stop_flag{false};
  
  // Producer thread
  std::thread producer([&source, &stop_flag]() {
    for (int i = 0; i < 100 && !stop_flag; ++i) {
      auto frame = MediaFrame::CreateShared(50, MediaType::VIDEO);
      frame->SetPts(base::Timestamp::Micros(i * 1000));
      source->QueueAccessUnit(frame);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  
  // Consumer thread
  std::thread consumer([&source, &stop_flag]() {
    int count = 0;
    while (count < 50 && !stop_flag) {
      std::shared_ptr<MediaFrame> frame;
      if (source->DequeueAccessUnit(frame) == OK) {
        count++;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });
  
  producer.join();
  stop_flag = true;
  
  // Signal EOS to unblock consumer if waiting
  source->SignalEOS(ERROR_END_OF_STREAM);
  consumer.join();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PacketSourceTest, QueueNullFrame) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  source->QueueAccessUnit(nullptr);
  
  status_t final_result;
  EXPECT_FALSE(source->HasBufferAvailable(&final_result));
}

TEST_F(PacketSourceTest, LargeFrameQueue) {
  auto source = std::make_shared<PacketSource>(video_meta_);
  
  // Queue many frames
  for (int i = 0; i < 1000; ++i) {
    auto frame = MediaFrame::CreateShared(1000, MediaType::VIDEO);
    frame->SetPts(base::Timestamp::Micros(i * 1000));
    source->QueueAccessUnit(frame);
  }
  
  status_t final_result;
  EXPECT_EQ(source->GetAvailableBufferCount(&final_result), 1000u);
  
  // Dequeue all
  for (int i = 0; i < 1000; ++i) {
    std::shared_ptr<MediaFrame> frame;
    EXPECT_EQ(source->DequeueAccessUnit(frame), OK);
    EXPECT_EQ(frame->pts().us(), i * 1000);
  }
}

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave