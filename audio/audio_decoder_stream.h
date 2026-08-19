/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_AUDIO_AUDIO_DECODER_STREAM_H_
#define MEDIA_AUDIO_AUDIO_DECODER_STREAM_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <queue>
#include <span>

#include "base/clock.h"
#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_factory.h"
#include "media/audio/pcm_ring_buffer.h"
#include "media/codec/codec.h"
#include "media/foundation/media_sink_base.h"

namespace ave::media {

class AudioDecoderStreamObserver {
 public:
  virtual ~AudioDecoderStreamObserver() = default;

  virtual void OnReceiveToDecoderInput(base::TimeDelta duration) = 0;
  virtual void OnDecoderInputDrop(size_t bytes) = 0;
  virtual void OnDecodedFrame(size_t bytes,
                              base::TimeDelta decode_duration) = 0;
  virtual void OnDecodeToPlayout(base::TimeDelta duration) = 0;
  virtual void OnPlaybackPcmDrop(size_t bytes) = 0;
  virtual void OnPlaybackQueueDuration(base::TimeDelta duration) = 0;
  virtual void OnDecoderError(status_t error) = 0;
};

class AudioDecoderStream final : public MediaFrameSink, public CodecCallback {
 public:
  struct Config {
    CodecId codec_id = CodecId::AVE_CODEC_ID_NONE;
    uint32_t output_sample_rate_hz = 48000;
    uint32_t max_playback_delay_ms = 40;
    size_t max_pending_encoded_frames = 4;
    std::shared_ptr<CodecConfig> decoder_config;
    base::Clock* clock = nullptr;
    base::TaskRunnerFactory* task_runner_factory = nullptr;
    AudioDecoderStreamObserver* observer = nullptr;
  };

  explicit AudioDecoderStream(const Config& config);
  ~AudioDecoderStream() override;

  bool Start();
  void Stop();
  bool running() const { return running_.load(); }

  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override;
  size_t ReadPcm(std::span<uint8_t> output);
  size_t queued_pcm_bytes() const { return pcm_ring_buffer_.queued_bytes(); }

  void OnInputBufferAvailable(size_t index) override;
  void OnOutputBufferAvailable(size_t index) override;
  void OnOutputFormatChanged(const std::shared_ptr<MediaMeta>& format) override;
  void OnError(status_t error) override;
  void OnFrameRendered(std::shared_ptr<Message> notify) override;

 private:
  struct PendingFrame {
    std::shared_ptr<MediaFrame> frame;
    base::Timestamp received_time = base::Timestamp::Zero();
  };

  void FeedDecoder();
  void ProcessOutput(size_t index);
  void UpdatePlaybackQueueMetric();

  Config config_;
  std::unique_ptr<base::TaskRunnerFactory> owned_task_runner_factory_;
  std::unique_ptr<base::TaskRunner> worker_runner_;
  std::shared_ptr<Codec> decoder_;
  PcmRingBuffer pcm_ring_buffer_;

  std::queue<size_t> decoder_input_indices_;
  std::deque<PendingFrame> pending_frames_;
  std::deque<base::Timestamp> decoder_start_times_;
  std::atomic<int64_t> first_queued_pcm_time_us_{0};
  std::atomic<bool> running_{false};
};

}  // namespace ave::media

#endif  // MEDIA_AUDIO_AUDIO_DECODER_STREAM_H_
