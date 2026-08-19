/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/audio_encoder_stream.h"

#include "media/audio/audio_decoder_stream.h"

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

class DecoderObserver final : public AudioDecoderStreamObserver {
 public:
  void OnReceiveToDecoderInput(base::TimeDelta /*duration*/) override {}
  void OnDecoderInputDrop(size_t /*bytes*/) override {}
  void OnDecodedFrame(size_t /*bytes*/,
                      base::TimeDelta /*decode_duration*/) override {
    std::scoped_lock lock(mutex_);
    decoded_ = true;
    condition_.notify_all();
  }
  void OnDecodeToPlayout(base::TimeDelta /*duration*/) override {}
  void OnPlaybackPcmDrop(size_t /*bytes*/) override {}
  void OnPlaybackQueueDuration(base::TimeDelta /*duration*/) override {}
  void OnDecoderError(status_t /*error*/) override {}

  bool WaitForDecodedFrame() {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(2),
                               [this]() { return decoded_; });
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  bool decoded_ = false;
};

void VerifyEncodeDecodeRoundTrip(uint32_t frame_duration_ms) {
  constexpr uint32_t kSampleRateHz = 48000;
  auto encoder_codec_config = std::make_shared<CodecConfig>();
  encoder_codec_config->format =
      MediaMeta::CreatePtr(MediaType::AUDIO, MediaMeta::FormatType::kTrack);
  encoder_codec_config->format->SetCodec(CodecId::AVE_CODEC_ID_OPUS)
      .SetSampleRate(kSampleRateHz)
      .SetChannelLayout(CHANNEL_LAYOUT_MONO)
      .SetSamplesPerChannel(kSampleRateHz * frame_duration_ms / 1000)
      .SetBitrate(32000);
  AudioEncoderStream encoder({
      .codec_id = CodecId::AVE_CODEC_ID_OPUS,
      .sample_rate_hz = kSampleRateHz,
      .frame_duration_ms = frame_duration_ms,
      .channel_layout = CHANNEL_LAYOUT_MONO,
      .encoder_config = std::move(encoder_codec_config),
      .clock = base::Clock::GetRealTimeClock(),
  });

  auto decoder_codec_config = std::make_shared<CodecConfig>();
  decoder_codec_config->format =
      MediaMeta::CreatePtr(MediaType::AUDIO, MediaMeta::FormatType::kTrack);
  decoder_codec_config->format->SetCodec(CodecId::AVE_CODEC_ID_OPUS)
      .SetSampleRate(kSampleRateHz)
      .SetChannelLayout(CHANNEL_LAYOUT_MONO);
  DecoderObserver observer;
  AudioDecoderStream decoder({
      .codec_id = CodecId::AVE_CODEC_ID_OPUS,
      .output_sample_rate_hz = kSampleRateHz,
      .max_playback_delay_ms = 40,
      .max_pending_encoded_frames = 4,
      .decoder_config = std::move(decoder_codec_config),
      .clock = base::Clock::GetRealTimeClock(),
      .observer = &observer,
  });
  encoder.AddOrUpdateSink(&decoder, {});
  ASSERT_TRUE(decoder.Start());
  ASSERT_TRUE(encoder.Start());

  const size_t samples = kSampleRateHz * frame_duration_ms / 1000;
  std::vector<int16_t> pcm(samples, 0);
  std::vector<uint8_t> output(samples * sizeof(int16_t));
  EXPECT_EQ(decoder.ReadPcm(output), 0U);
  encoder.DeliverPcm(std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(pcm.data()), output.size()));
  ASSERT_TRUE(observer.WaitForDecodedFrame());
  EXPECT_EQ(decoder.ReadPcm(output), output.size());
  EXPECT_EQ(decoder.queued_pcm_bytes(), 0U);

  encoder.Stop();
  encoder.RemoveSink(&decoder);
  decoder.Stop();
}

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

TEST(AudioDecoderStreamTest, PlaysTenMillisecondFramesAfterUnderflow) {
  VerifyEncodeDecodeRoundTrip(10);
}

TEST(AudioDecoderStreamTest, PlaysTwentyMillisecondFramesAfterUnderflow) {
  VerifyEncodeDecodeRoundTrip(20);
}

}  // namespace
}  // namespace ave::media
