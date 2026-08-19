/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/audio_encoder_stream.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "base/clock.h"
#include "media/foundation/media_sink_base.h"
#include "test/gtest.h"

namespace ave::media {
namespace {

class EncodedFrameSink final : public MediaFrameSink {
 public:
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override {
    std::scoped_lock lock(mutex_);
    frames_.push_back(frame);
    condition_.notify_all();
  }

  bool WaitForFrame() {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(2),
                               [this]() { return !frames_.empty(); });
  }

  std::shared_ptr<MediaFrame> first_frame() const {
    std::scoped_lock lock(mutex_);
    return frames_.empty() ? nullptr : frames_.front();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::vector<std::shared_ptr<MediaFrame>> frames_;
};

TEST(AudioEncoderStreamTest, PublishesOneEncodedFrameToTwoSinks) {
  constexpr uint32_t kSampleRateHz = 16000;
  constexpr uint32_t kFrameDurationMs = 20;
  auto codec_config = std::make_shared<CodecConfig>();
  codec_config->format =
      MediaMeta::CreatePtr(MediaType::AUDIO, MediaMeta::FormatType::kTrack);
  codec_config->format->SetCodec(CodecId::AVE_CODEC_ID_OPUS)
      .SetSampleRate(kSampleRateHz)
      .SetChannelLayout(CHANNEL_LAYOUT_MONO)
      .SetSamplesPerChannel(kSampleRateHz * kFrameDurationMs / 1000)
      .SetBitrate(32000);

  AudioEncoderStream::Config config{
      .codec_id = CodecId::AVE_CODEC_ID_OPUS,
      .sample_rate_hz = kSampleRateHz,
      .frame_duration_ms = kFrameDurationMs,
      .channel_layout = CHANNEL_LAYOUT_MONO,
      .encoder_config = std::move(codec_config),
      .clock = base::Clock::GetRealTimeClock(),
  };
  AudioEncoderStream encoder(config);
  EncodedFrameSink first_sink;
  EncodedFrameSink second_sink;
  encoder.AddOrUpdateSink(&first_sink, {});
  encoder.AddOrUpdateSink(&second_sink, {});
  ASSERT_TRUE(encoder.Start());

  std::vector<int16_t> pcm(kSampleRateHz * kFrameDurationMs / 1000, 0);
  encoder.DeliverPcm(
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pcm.data()),
                               pcm.size() * sizeof(int16_t)));

  ASSERT_TRUE(first_sink.WaitForFrame());
  ASSERT_TRUE(second_sink.WaitForFrame());
  const std::shared_ptr<MediaFrame> frame = first_sink.first_frame();
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame, second_sink.first_frame());
  EXPECT_EQ(frame->stream_type(), MediaType::AUDIO);
  EXPECT_EQ(frame->codec(), CodecId::AVE_CODEC_ID_OPUS);
  EXPECT_EQ(frame->sample_rate(), kSampleRateHz);
  EXPECT_EQ(frame->duration(), base::TimeDelta::Millis(kFrameDurationMs));
  EXPECT_GT(frame->size(), 0U);

  encoder.Stop();
  encoder.RemoveSink(&first_sink);
  encoder.RemoveSink(&second_sink);
}

}  // namespace
}  // namespace ave::media
