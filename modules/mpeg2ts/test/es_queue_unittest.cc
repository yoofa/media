/*
 * es_queue_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Comprehensive unit tests for ESQueue
 */

#include "../es_queue.h"

#include <gtest/gtest.h>
#include <vector>

#include "foundation/media_frame.h"
#include "foundation/media_meta.h"

namespace ave {
namespace media {
namespace mpeg2ts {

class ESQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Default setup
  }
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ESQueueTest, ConstructH264) {
  ESQueue queue(ESQueue::Mode::H264);
  EXPECT_EQ(queue.GetFormat(), nullptr);  // No format until data is parsed
}

TEST_F(ESQueueTest, ConstructAAC) {
  ESQueue queue(ESQueue::Mode::AAC);
  EXPECT_EQ(queue.GetFormat(), nullptr);
}

TEST_F(ESQueueTest, ConstructWithFlags) {
  ESQueue queue(ESQueue::Mode::H264, ESQueue::kFlag_AlignedData);
  EXPECT_FALSE(queue.IsScrambled());
}

TEST_F(ESQueueTest, ScrambledFlag) {
  ESQueue queue(ESQueue::Mode::H264, ESQueue::kFlag_ScrambledData);
  EXPECT_TRUE(queue.IsScrambled());
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F(ESQueueTest, ClearWithoutFormat) {
  ESQueue queue(ESQueue::Mode::H264);
  
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x67};  // H.264 SPS NAL
  queue.AppendData(data.data(), data.size(), 1000, 0, 0);
  
  queue.Clear(false);
  auto au = queue.DequeueAccessUnit();
  EXPECT_EQ(au, nullptr);
}

TEST_F(ESQueueTest, ClearWithFormat) {
  ESQueue queue(ESQueue::Mode::H264);
  
  // Append some data to potentially create format
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x67};
  queue.AppendData(data.data(), data.size(), 1000, 0, 0);
  
  queue.Clear(true);
  EXPECT_EQ(queue.GetFormat(), nullptr);
}

// ============================================================================
// EOS Tests
// ============================================================================

TEST_F(ESQueueTest, SignalEOS) {
  ESQueue queue(ESQueue::Mode::H264);
  
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01, 0x02};
  queue.AppendData(data.data(), data.size(), 1000, 0, 0);
  
  queue.SignalEOS();
  
  // Should be able to dequeue remaining data
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
}

// ============================================================================
// Aligned Data Mode Tests
// ============================================================================

TEST_F(ESQueueTest, AlignedDataMode) {
  ESQueue queue(ESQueue::Mode::H264, ESQueue::kFlag_AlignedData);
  
  // In aligned mode, each append is treated as complete access unit
  std::vector<uint8_t> frame1 = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
  queue.AppendData(frame1.data(), frame1.size(), 1000, 0, 0);
  
  auto au1 = queue.DequeueAccessUnit();
  EXPECT_NE(au1, nullptr);
  EXPECT_EQ(au1->size(), frame1.size());
  EXPECT_EQ(au1->pts().us(), 1000);
}

TEST_F(ESQueueTest, AlignedDataMultipleFrames) {
  ESQueue queue(ESQueue::Mode::AAC, ESQueue::kFlag_AlignedData);
  
  std::vector<uint8_t> frame1(100, 0xAA);
  std::vector<uint8_t> frame2(150, 0xBB);
  
  queue.AppendData(frame1.data(), frame1.size(), 1000, 0, 0);
  queue.AppendData(frame2.data(), frame2.size(), 2000, 0, 0);
  
  auto au1 = queue.DequeueAccessUnit();
  EXPECT_NE(au1, nullptr);
  EXPECT_EQ(au1->size(), 100u);
  
  auto au2 = queue.DequeueAccessUnit();
  EXPECT_NE(au2, nullptr);
  EXPECT_EQ(au2->size(), 150u);
}

// ============================================================================
// H.264 Parsing Tests
// ============================================================================

TEST_F(ESQueueTest, H264StartCodeDetection) {
  ESQueue queue(ESQueue::Mode::H264);
  
  // H.264 NAL unit with 4-byte start code
  std::vector<uint8_t> data = {
    0x00, 0x00, 0x00, 0x01,  // start code
    0x67, 0x42, 0x00, 0x1E,  // SPS NAL unit
    0x00, 0x00, 0x00, 0x01,  // next start code
    0x68, 0xCE, 0x38, 0x80   // PPS NAL unit
  };
  
  queue.AppendData(data.data(), data.size(), 1000, 0, 0);
  queue.SignalEOS();
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
}

TEST_F(ESQueueTest, H264ThreeByteStartCode) {
  ESQueue queue(ESQueue::Mode::H264);
  
  // H.264 NAL with 3-byte start code
  std::vector<uint8_t> data = {
    0x00, 0x00, 0x01,        // 3-byte start code
    0x65, 0x88, 0x84, 0x00,  // IDR slice
    0x00, 0x00, 0x01,        // next start code
    0x41, 0x9A, 0x21, 0x4C   // Non-IDR slice
  };
  
  queue.AppendData(data.data(), data.size(), 2000, 0, 0);
  queue.SignalEOS();
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
  EXPECT_EQ(au->stream_type(), MediaType::VIDEO);
}

TEST_F(ESQueueTest, H264IncompleteNAL) {
  ESQueue queue(ESQueue::Mode::H264);
  
  // Only start code, no complete NAL
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x67};
  queue.AppendData(data.data(), data.size(), 1000, 0, 0);
  
  // Should return nullptr without EOS
  auto au = queue.DequeueAccessUnit();
  EXPECT_EQ(au, nullptr);
  
  // Should return data after EOS
  queue.SignalEOS();
  au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
}

// ============================================================================
// AAC/ADTS Parsing Tests
// ============================================================================

TEST_F(ESQueueTest, AACAdtsSyncWord) {
  ESQueue queue(ESQueue::Mode::AAC);
  
  // ADTS frame with sync word 0xFFF
  std::vector<uint8_t> adts_frame = {
    0xFF, 0xF1,              // Sync word + MPEG-4 + no CRC
    0x50, 0x80, 0x04, 0x3F,  // Sample rate index, channels, frame length
    0xFC,                     // Remaining header
    0x01, 0x02, 0x03, 0x04   // AAC payload
  };
  
  queue.AppendData(adts_frame.data(), adts_frame.size(), 3000, 0, 0);
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
  EXPECT_EQ(au->stream_type(), MediaType::AUDIO);
  EXPECT_EQ(au->pts().us(), 3000);
}

TEST_F(ESQueueTest, AACMultipleFrames) {
  ESQueue queue(ESQueue::Mode::AAC);
  
  // Two ADTS frames back-to-back
  std::vector<uint8_t> data = {
    // Frame 1
    0xFF, 0xF1, 0x50, 0x80, 0x04, 0x3F, 0xFC,
    0xAA, 0xBB, 0xCC, 0xDD,
    // Frame 2
    0xFF, 0xF1, 0x50, 0x80, 0x04, 0x3F, 0xFC,
    0x11, 0x22, 0x33, 0x44
  };
  
  queue.AppendData(data.data(), data.size(), 4000, 0, 0);
  
  auto au1 = queue.DequeueAccessUnit();
  EXPECT_NE(au1, nullptr);
  
  auto au2 = queue.DequeueAccessUnit();
  EXPECT_NE(au2, nullptr);
}

TEST_F(ESQueueTest, AACInvalidSyncWord) {
  ESQueue queue(ESQueue::Mode::AAC);
  
  // Invalid sync word
  std::vector<uint8_t> data = {
    0xFF, 0xE1,  // Invalid (should be 0xFFF)
    0x50, 0x80, 0x04, 0x3F, 0xFC,
    0x01, 0x02, 0x03, 0x04
  };
  
  queue.AppendData(data.data(), data.size(), 5000, 0, 0);
  queue.SignalEOS();
  
  auto au = queue.DequeueAccessUnit();
  // Should still return data at EOS, even if invalid
  EXPECT_NE(au, nullptr);
}

// ============================================================================
// Timestamp Tests
// ============================================================================

TEST_F(ESQueueTest, TimestampPropagation) {
  ESQueue queue(ESQueue::Mode::H264, ESQueue::kFlag_AlignedData);
  
  int64_t expected_pts = 123456789;
  std::vector<uint8_t> data(100, 0x42);
  
  queue.AppendData(data.data(), data.size(), expected_pts, 0, 0);
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
  EXPECT_EQ(au->pts().us(), expected_pts);
}

TEST_F(ESQueueTest, NegativeTimestamp) {
  ESQueue queue(ESQueue::Mode::H264, ESQueue::kFlag_AlignedData);
  
  std::vector<uint8_t> data(50, 0x55);
  queue.AppendData(data.data(), data.size(), -1, 0, 0);  // No timestamp
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
  EXPECT_LT(au->pts().us(), 0);  // Should preserve invalid timestamp
}

// ============================================================================
// CAS (Conditional Access) Tests
// ============================================================================

TEST_F(ESQueueTest, SetCasInfo) {
  ESQueue queue(ESQueue::Mode::H264);
  
  std::vector<uint8_t> session_id = {0x01, 0x02, 0x03, 0x04};
  queue.SetCasInfo(0x1234, session_id);
  
  // CAS info is set, but doesn't affect basic operations
  std::vector<uint8_t> data(100, 0xAA);
  queue.AppendData(data.data(), data.size(), 1000, 0, 0);
  queue.SignalEOS();
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ESQueueTest, EmptyData) {
  ESQueue queue(ESQueue::Mode::H264);
  
  queue.AppendData(nullptr, 0, 1000, 0, 0);
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_EQ(au, nullptr);
}

TEST_F(ESQueueTest, VeryLargeData) {
  ESQueue queue(ESQueue::Mode::H264, ESQueue::kFlag_AlignedData);
  
  // Test with large buffer (> 8KB to trigger reallocation)
  std::vector<uint8_t> large_data(10000, 0xCC);
  queue.AppendData(large_data.data(), large_data.size(), 6000, 0, 0);
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
  EXPECT_EQ(au->size(), large_data.size());
}

TEST_F(ESQueueTest, MultipleAppendBeforeDequeue) {
  ESQueue queue(ESQueue::Mode::H264, ESQueue::kFlag_AlignedData);
  
  for (int i = 0; i < 10; ++i) {
    std::vector<uint8_t> data(100, static_cast<uint8_t>(i));
    queue.AppendData(data.data(), data.size(), i * 1000, 0, 0);
  }
  
  for (int i = 0; i < 10; ++i) {
    auto au = queue.DequeueAccessUnit();
    EXPECT_NE(au, nullptr);
    EXPECT_EQ(au->pts().us(), i * 1000);
  }
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_EQ(au, nullptr);
}

// ============================================================================
// Format Detection Tests
// ============================================================================

TEST_F(ESQueueTest, H264FormatGeneration) {
  ESQueue queue(ESQueue::Mode::H264);
  
  // After parsing H.264 data, format should be set
  std::vector<uint8_t> data = {
    0x00, 0x00, 0x00, 0x01,
    0x67, 0x42, 0x00, 0x1E,
    0x00, 0x00, 0x00, 0x01,
    0x65, 0x88, 0x84, 0x00
  };
  
  queue.AppendData(data.data(), data.size(), 7000, 0, 0);
  queue.SignalEOS();
  
  auto au = queue.DequeueAccessUnit();
  EXPECT_NE(au, nullptr);
  
  auto format = queue.GetFormat();
  EXPECT_NE(format, nullptr);
  EXPECT_EQ(format->stream_type(), MediaType::VIDEO);
}

// ============================================================================
// Unsupported Modes Tests
// ============================================================================

TEST_F(ESQueueTest, AC3ModeWarning) {
  ESQueue queue(ESQueue::Mode::AC3);
  
  std::vector<uint8_t> data(100, 0xFF);
  queue.AppendData(data.data(), data.size(), 8000, 0, 0);
  
  // AC3 is not fully implemented, should return nullptr
  auto au = queue.DequeueAccessUnit();
  EXPECT_EQ(au, nullptr);
}

TEST_F(ESQueueTest, MPEGVideoMode) {
  ESQueue queue(ESQueue::Mode::MPEG_VIDEO);
  
  std::vector<uint8_t> data(100, 0xDD);
  queue.AppendData(data.data(), data.size(), 9000, 0, 0);
  
  // MPEG video not fully implemented
  auto au = queue.DequeueAccessUnit();
  EXPECT_EQ(au, nullptr);
}

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave