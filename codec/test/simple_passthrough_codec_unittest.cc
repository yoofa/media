/*
 * simple_passthrough_codec_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/codec/simple_passthrough_codec.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <thread>

#include "media/codec/codec.h"
#include "media/foundation/media_meta.h"
#include "test/gtest.h"

namespace ave {
namespace media {

namespace {

/**
 * @brief Helper callback that tracks codec events for testing.
 */
class TestCallback : public CodecCallback {
 public:
  void OnInputBufferAvailable(size_t index) override {
    input_available_count_++;
    last_input_index_ = index;
  }
  void OnOutputBufferAvailable(size_t index) override {
    output_available_count_++;
    last_output_index_ = index;
  }
  void OnOutputFormatChanged(
      const std::shared_ptr<MediaMeta>& format) override {
    format_changed_count_++;
  }
  void OnError(status_t error) override {
    error_count_++;
    last_error_ = error;
  }
  void OnFrameRendered(std::shared_ptr<Message> notify) override {
    frame_rendered_count_++;
  }

  std::atomic<int> input_available_count_{0};
  std::atomic<int> output_available_count_{0};
  std::atomic<int> format_changed_count_{0};
  std::atomic<int> error_count_{0};
  std::atomic<int> frame_rendered_count_{0};
  std::atomic<size_t> last_input_index_{0};
  std::atomic<size_t> last_output_index_{0};
  std::atomic<status_t> last_error_{OK};
};

std::shared_ptr<CodecConfig> CreateTestConfig() {
  auto config = std::make_shared<CodecConfig>();
  config->format =
      MediaMeta::CreatePtr(MediaType::AUDIO, MediaMeta::FormatType::kSample);
  return config;
}

}  // namespace

class SimplePassthroughCodecTest : public ::testing::Test {
 protected:
  void SetUp() override {
    codec_ = std::make_shared<SimplePassthroughCodec>(false);
    callback_ = std::make_unique<TestCallback>();
  }

  void TearDown() override {
    codec_.reset();
    callback_.reset();
  }

  std::shared_ptr<SimplePassthroughCodec> codec_;
  std::unique_ptr<TestCallback> callback_;
};

TEST_F(SimplePassthroughCodecTest, ConfigureSucceeds) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
}

TEST_F(SimplePassthroughCodecTest, ConfigureWithNullFormatFails) {
  auto config = std::make_shared<CodecConfig>();
  config->format = nullptr;
  EXPECT_NE(codec_->Configure(config), OK);
}

TEST_F(SimplePassthroughCodecTest, ConfigureWithNullConfigFails) {
  auto config = std::make_shared<CodecConfig>();
  EXPECT_NE(codec_->Configure(config), OK);
}

TEST_F(SimplePassthroughCodecTest, DoubleConfigureFails) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_NE(codec_->Configure(config), OK);
}

TEST_F(SimplePassthroughCodecTest, StartAfterConfigure) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);
}

TEST_F(SimplePassthroughCodecTest, StartWithoutConfigureFails) {
  EXPECT_NE(codec_->Start(), OK);
}

TEST_F(SimplePassthroughCodecTest, StopAfterStart) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);
  // Give the task runner time to process
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(codec_->Stop(), OK);
}

TEST_F(SimplePassthroughCodecTest, SetCallback) {
  EXPECT_EQ(codec_->SetCallback(callback_.get()), OK);
}

TEST_F(SimplePassthroughCodecTest, DequeueInputBufferAfterStart) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);

  ssize_t index = codec_->DequeueInputBuffer(0);
  EXPECT_GE(index, 0);
}

TEST_F(SimplePassthroughCodecTest, GetInputBuffer) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);

  std::shared_ptr<CodecBuffer> buffer;
  EXPECT_EQ(codec_->GetInputBuffer(0, buffer), OK);
  EXPECT_NE(buffer, nullptr);
}

TEST_F(SimplePassthroughCodecTest, GetInputBufferInvalidIndex) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);

  std::shared_ptr<CodecBuffer> buffer;
  EXPECT_NE(codec_->GetInputBuffer(9999, buffer), OK);
}

TEST_F(SimplePassthroughCodecTest, GetOutputBuffer) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);

  std::shared_ptr<CodecBuffer> buffer;
  EXPECT_EQ(codec_->GetOutputBuffer(0, buffer), OK);
  EXPECT_NE(buffer, nullptr);
}

TEST_F(SimplePassthroughCodecTest, PassthroughDataIntegrity) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->SetCallback(callback_.get()), OK);
  EXPECT_EQ(codec_->Start(), OK);

  // Get input buffer and write test data
  ssize_t input_idx = codec_->DequeueInputBuffer(100);
  ASSERT_GE(input_idx, 0);

  std::shared_ptr<CodecBuffer> input_buffer;
  EXPECT_EQ(
      codec_->GetInputBuffer(static_cast<size_t>(input_idx), input_buffer), OK);

  const char* test_data = "PassthroughTestData12345";
  size_t data_len = std::strlen(test_data);
  std::memcpy(input_buffer->data(), test_data, data_len);
  input_buffer->SetRange(0, data_len);

  // Queue input
  EXPECT_EQ(codec_->QueueInputBuffer(static_cast<size_t>(input_idx)), OK);

  // Wait for processing
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Dequeue output
  ssize_t output_idx = codec_->DequeueOutputBuffer(100);
  ASSERT_GE(output_idx, 0);

  std::shared_ptr<CodecBuffer> output_buffer;
  EXPECT_EQ(
      codec_->GetOutputBuffer(static_cast<size_t>(output_idx), output_buffer),
      OK);

  // Verify data integrity
  EXPECT_EQ(output_buffer->size(), data_len);
  EXPECT_EQ(std::memcmp(output_buffer->data(), test_data, data_len), 0);

  // Release output
  EXPECT_EQ(codec_->ReleaseOutputBuffer(static_cast<size_t>(output_idx), false),
            OK);
  codec_->Stop();
}

TEST_F(SimplePassthroughCodecTest, MultipleFramePassthrough) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);

  const int kNumFrames = 5;
  for (int i = 0; i < kNumFrames; ++i) {
    ssize_t input_idx = codec_->DequeueInputBuffer(200);
    ASSERT_GE(input_idx, 0) << "Failed to dequeue input buffer at frame " << i;

    std::shared_ptr<CodecBuffer> input_buffer;
    EXPECT_EQ(
        codec_->GetInputBuffer(static_cast<size_t>(input_idx), input_buffer),
        OK);

    // Write unique data per frame
    uint8_t fill_value = static_cast<uint8_t>(i + 1);
    std::memset(input_buffer->data(), fill_value, 100);
    input_buffer->SetRange(0, 100);
    EXPECT_EQ(codec_->QueueInputBuffer(static_cast<size_t>(input_idx)), OK);

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ssize_t output_idx = codec_->DequeueOutputBuffer(200);
    ASSERT_GE(output_idx, 0) << "Failed to dequeue output at frame " << i;

    std::shared_ptr<CodecBuffer> output_buffer;
    EXPECT_EQ(
        codec_->GetOutputBuffer(static_cast<size_t>(output_idx), output_buffer),
        OK);

    EXPECT_EQ(output_buffer->size(), 100u);
    EXPECT_EQ(output_buffer->data()[0], fill_value);
    EXPECT_EQ(output_buffer->data()[99], fill_value);

    EXPECT_EQ(
        codec_->ReleaseOutputBuffer(static_cast<size_t>(output_idx), false),
        OK);
  }

  codec_->Stop();
}

TEST_F(SimplePassthroughCodecTest, FlushDuringOperation) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);

  // Queue some data
  ssize_t input_idx = codec_->DequeueInputBuffer(100);
  ASSERT_GE(input_idx, 0);

  std::shared_ptr<CodecBuffer> input_buffer;
  codec_->GetInputBuffer(static_cast<size_t>(input_idx), input_buffer);
  std::memset(input_buffer->data(), 0xFF, 64);
  input_buffer->SetRange(0, 64);
  codec_->QueueInputBuffer(static_cast<size_t>(input_idx));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Flush
  EXPECT_EQ(codec_->Flush(), OK);

  codec_->Stop();
}

TEST_F(SimplePassthroughCodecTest, ResetReturnsToConfigured) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(codec_->Stop(), OK);
  EXPECT_EQ(codec_->Reset(), OK);
  // After reset, should be able to start again
  EXPECT_EQ(codec_->Start(), OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  codec_->Stop();
}

TEST_F(SimplePassthroughCodecTest, ReleaseOutputInvalidIndex) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);

  EXPECT_NE(codec_->ReleaseOutputBuffer(9999, false), OK);
  codec_->Stop();
}

TEST_F(SimplePassthroughCodecTest, QueueInputInvalidIndex) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);

  EXPECT_NE(codec_->QueueInputBuffer(9999), OK);
  codec_->Stop();
}

TEST_F(SimplePassthroughCodecTest, EncoderMode) {
  auto encoder = std::make_shared<SimplePassthroughCodec>(true);
  EXPECT_TRUE(encoder->IsEncoder());

  auto config = CreateTestConfig();
  EXPECT_EQ(encoder->Configure(config), OK);
  EXPECT_EQ(encoder->Start(), OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  encoder->Stop();
}

TEST_F(SimplePassthroughCodecTest, EmptyBufferPassthrough) {
  auto config = CreateTestConfig();
  EXPECT_EQ(codec_->Configure(config), OK);
  EXPECT_EQ(codec_->Start(), OK);

  ssize_t input_idx = codec_->DequeueInputBuffer(100);
  ASSERT_GE(input_idx, 0);

  std::shared_ptr<CodecBuffer> input_buffer;
  codec_->GetInputBuffer(static_cast<size_t>(input_idx), input_buffer);
  input_buffer->SetRange(0, 0);  // empty
  EXPECT_EQ(codec_->QueueInputBuffer(static_cast<size_t>(input_idx)), OK);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ssize_t output_idx = codec_->DequeueOutputBuffer(100);
  ASSERT_GE(output_idx, 0);

  std::shared_ptr<CodecBuffer> output_buffer;
  codec_->GetOutputBuffer(static_cast<size_t>(output_idx), output_buffer);
  EXPECT_EQ(output_buffer->size(), 0u);

  codec_->ReleaseOutputBuffer(static_cast<size_t>(output_idx), false);
  codec_->Stop();
}

}  // namespace media
}  // namespace ave
