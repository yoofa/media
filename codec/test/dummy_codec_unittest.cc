/*
 * dummy_codec_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "dummy_codec.h"
#include "dummy_codec_factory.h"

#include <cstring>
#include <memory>
#include <thread>

#include "media/codec/codec.h"
#include "media/codec/codec_factory.h"
#include "media/foundation/media_meta.h"
#include "test/gtest.h"

namespace ave {
namespace media {
namespace test {

class DummyCodecTest : public ::testing::Test {
 protected:
  void SetUp() override {
    codec_ = std::make_shared<DummyCodec>(false);
    auto config = std::make_shared<CodecConfig>();
    config->format =
        MediaMeta::CreatePtr(MediaType::AUDIO, MediaMeta::FormatType::kSample);
    ASSERT_EQ(codec_->Configure(config), OK);
  }

  void TearDown() override { codec_.reset(); }

  std::shared_ptr<DummyCodec> codec_;
};

TEST_F(DummyCodecTest, ConfigureSucceeds) {
  auto codec = std::make_shared<DummyCodec>(false);
  auto config = std::make_shared<CodecConfig>();
  config->format = MediaMeta::CreatePtr();
  EXPECT_EQ(codec->Configure(config), OK);
}

TEST_F(DummyCodecTest, StartStop) {
  EXPECT_EQ(codec_->Start(), OK);
  EXPECT_EQ(codec_->Stop(), OK);
}

TEST_F(DummyCodecTest, DoubleStartFails) {
  EXPECT_EQ(codec_->Start(), OK);
  EXPECT_EQ(codec_->Start(), INVALID_OPERATION);
  codec_->Stop();
}

TEST_F(DummyCodecTest, DequeueInputBuffer) {
  EXPECT_EQ(codec_->Start(), OK);
  ssize_t index = codec_->DequeueInputBuffer(0);
  EXPECT_GE(index, 0);
  codec_->Stop();
}

TEST_F(DummyCodecTest, DequeueInputBufferWhenNotStarted) {
  ssize_t index = codec_->DequeueInputBuffer(0);
  EXPECT_LT(index, 0);
}

TEST_F(DummyCodecTest, GetInputBuffer) {
  std::shared_ptr<CodecBuffer> buffer;
  EXPECT_EQ(codec_->GetInputBuffer(0, buffer), OK);
  EXPECT_NE(buffer, nullptr);
}

TEST_F(DummyCodecTest, GetInputBufferInvalidIndex) {
  std::shared_ptr<CodecBuffer> buffer;
  EXPECT_NE(codec_->GetInputBuffer(9999, buffer), OK);
}

TEST_F(DummyCodecTest, GetOutputBuffer) {
  std::shared_ptr<CodecBuffer> buffer;
  EXPECT_EQ(codec_->GetOutputBuffer(0, buffer), OK);
  EXPECT_NE(buffer, nullptr);
}

TEST_F(DummyCodecTest, GetOutputBufferInvalidIndex) {
  std::shared_ptr<CodecBuffer> buffer;
  EXPECT_NE(codec_->GetOutputBuffer(9999, buffer), OK);
}

TEST_F(DummyCodecTest, QueueInputInvalidIndex) {
  EXPECT_EQ(codec_->Start(), OK);
  EXPECT_EQ(codec_->QueueInputBuffer(9999), INVALID_OPERATION);
  codec_->Stop();
}

TEST_F(DummyCodecTest, PassthroughData) {
  EXPECT_EQ(codec_->Start(), OK);

  // Dequeue input
  ssize_t input_idx = codec_->DequeueInputBuffer(0);
  ASSERT_GE(input_idx, 0);

  // Write data
  std::shared_ptr<CodecBuffer> input_buffer;
  codec_->GetInputBuffer(static_cast<size_t>(input_idx), input_buffer);
  const char* test_data = "DummyCodecTest";
  size_t len = std::strlen(test_data);
  std::memcpy(input_buffer->data(), test_data, len);
  input_buffer->SetRange(0, len);

  // Queue input
  EXPECT_EQ(codec_->QueueInputBuffer(static_cast<size_t>(input_idx)), OK);

  // Wait for async processing
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Dequeue output
  ssize_t output_idx = codec_->DequeueOutputBuffer(0);
  ASSERT_GE(output_idx, 0);

  std::shared_ptr<CodecBuffer> output_buffer;
  codec_->GetOutputBuffer(static_cast<size_t>(output_idx), output_buffer);

  EXPECT_EQ(output_buffer->size(), len);
  EXPECT_EQ(std::memcmp(output_buffer->data(), test_data, len), 0);

  EXPECT_EQ(codec_->ReleaseOutputBuffer(static_cast<size_t>(output_idx), false),
            OK);
  codec_->Stop();
}

TEST_F(DummyCodecTest, ResetClearsQueues) {
  EXPECT_EQ(codec_->Start(), OK);

  ssize_t input_idx = codec_->DequeueInputBuffer(0);
  ASSERT_GE(input_idx, 0);

  std::shared_ptr<CodecBuffer> input_buffer;
  codec_->GetInputBuffer(static_cast<size_t>(input_idx), input_buffer);
  input_buffer->SetRange(0, 10);
  codec_->QueueInputBuffer(static_cast<size_t>(input_idx));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  codec_->Stop();
  EXPECT_EQ(codec_->Reset(), OK);

  // After reset, output queue should be empty
  ssize_t output_idx = codec_->DequeueOutputBuffer(0);
  EXPECT_LT(output_idx, 0);
}

TEST_F(DummyCodecTest, FlushClearsQueues) {
  EXPECT_EQ(codec_->Start(), OK);
  EXPECT_EQ(codec_->Flush(), OK);
  codec_->Stop();
}

TEST_F(DummyCodecTest, ReleaseOutputInvalidIndex) {
  EXPECT_EQ(codec_->Start(), OK);
  EXPECT_EQ(codec_->ReleaseOutputBuffer(9999, false), INVALID_OPERATION);
  codec_->Stop();
}

TEST_F(DummyCodecTest, DequeueOutputWhenEmpty) {
  EXPECT_EQ(codec_->Start(), OK);
  ssize_t index = codec_->DequeueOutputBuffer(0);
  EXPECT_LT(index, 0);
  codec_->Stop();
}

// DummyCodecFactory tests
class DummyCodecFactoryTest : public ::testing::Test {
 protected:
  DummyCodecFactory factory_;
};

TEST_F(DummyCodecFactoryTest, GetSupportedCodecs) {
  auto codecs = factory_.GetSupportedCodecs();
  EXPECT_GT(codecs.size(), 0u);
}

TEST_F(DummyCodecFactoryTest, CreateCodecByType) {
  auto codec = factory_.CreateCodecByType(CodecId::AVE_CODEC_ID_H264, false);
  EXPECT_NE(codec, nullptr);
}

TEST_F(DummyCodecFactoryTest, CreateCodecByName) {
  auto decoder = factory_.CreateCodecByName("dummy_decoder");
  EXPECT_NE(decoder, nullptr);

  auto encoder = factory_.CreateCodecByName("dummy_encoder");
  EXPECT_NE(encoder, nullptr);

  auto unknown = factory_.CreateCodecByName("nonexistent");
  EXPECT_EQ(unknown, nullptr);
}

TEST_F(DummyCodecFactoryTest, CreateCodecByMime) {
  auto video = factory_.CreateCodecByMime("video/dummy", false);
  EXPECT_NE(video, nullptr);

  auto audio = factory_.CreateCodecByMime("audio/dummy", true);
  EXPECT_NE(audio, nullptr);

  auto unknown = factory_.CreateCodecByMime("video/unknown", false);
  EXPECT_EQ(unknown, nullptr);
}

TEST_F(DummyCodecFactoryTest, FactoryName) {
  EXPECT_EQ(factory_.name(), "dummy");
}

TEST_F(DummyCodecFactoryTest, FactoryPriority) {
  EXPECT_EQ(factory_.priority(), 0);
}

}  // namespace test
}  // namespace media
}  // namespace ave
