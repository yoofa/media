/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_AUDIO_AUDIO_ENCODER_STREAM_H_
#define MEDIA_AUDIO_AUDIO_ENCODER_STREAM_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <queue>
#include <span>
#include <vector>

#include "base/clock.h"
#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_factory.h"
#include "base/units/timestamp.h"
#include "media/audio/channel_layout.h"
#include "media/codec/codec.h"
#include "media/codec/codec_id.h"
#include "media/foundation/async_media_frame_source.h"

namespace ave::media {

class AudioEncoderStreamObserver {
 public:
  virtual ~AudioEncoderStreamObserver() = default;

  virtual void OnCaptureToEncoderInput(base::TimeDelta duration) = 0;
  virtual void OnEncodedFrame(size_t bytes,
                              base::TimeDelta encode_duration) = 0;
  virtual void OnEncoderError(status_t error) = 0;
};

// Converts PCM input into encoded MediaFrames. Transport-specific packet types
// deliberately do not appear in this class.
class AudioEncoderStream final : public AsyncMediaFrameSource,
                                 public CodecCallback {
 public:
  struct Config {
    CodecId codec_id = CodecId::AVE_CODEC_ID_NONE;
    uint32_t sample_rate_hz = 16000;
    uint32_t frame_duration_ms = 20;
    ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
    std::shared_ptr<CodecConfig> encoder_config;
    base::Clock* clock = nullptr;
    base::TaskRunnerFactory* task_runner_factory = nullptr;
    AudioEncoderStreamObserver* observer = nullptr;
  };

  explicit AudioEncoderStream(const Config& config);
  ~AudioEncoderStream() override;

  bool Start();
  void Stop();
  bool running() const { return running_.load(); }

  void DeliverPcm(std::span<const uint8_t> pcm);

  // CodecCallback implementation.
  void OnInputBufferAvailable(size_t index) override;
  void OnOutputBufferAvailable(size_t index) override;
  void OnOutputFormatChanged(const std::shared_ptr<MediaMeta>& format) override;
  void OnError(status_t error) override;
  void OnFrameRendered(std::shared_ptr<Message> notify) override;

 private:
  struct PcmChunkTiming {
    size_t remaining_bytes = 0;
    base::Timestamp arrival_time = base::Timestamp::Zero();
  };

  struct EncodedFrameTiming {
    base::Timestamp pts = base::Timestamp::Zero();
    base::Timestamp encode_start = base::Timestamp::Zero();
  };

  void FeedEncoder();
  void ProcessOutput(size_t index);
  base::Timestamp ConsumePcmTiming(size_t bytes);
  std::shared_ptr<MediaFrame> CreateEncodedFrame(
      CodecBuffer& codec_buffer,
      const EncodedFrameTiming& timing) const;

  Config config_;
  std::unique_ptr<base::TaskRunnerFactory> owned_task_runner_factory_;
  std::unique_ptr<base::TaskRunner> worker_runner_;
  std::shared_ptr<Codec> encoder_;
  std::shared_ptr<MediaMeta> output_format_;

  std::queue<size_t> encoder_input_indices_;
  std::vector<uint8_t> pcm_buffer_;
  std::deque<PcmChunkTiming> pcm_chunk_timings_;
  std::deque<EncodedFrameTiming> encoded_frame_timings_;
  std::optional<base::Timestamp> next_pts_;

  std::atomic<bool> running_{false};
};

}  // namespace ave::media

#endif  // MEDIA_AUDIO_AUDIO_ENCODER_STREAM_H_
