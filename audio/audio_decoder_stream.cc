/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/audio_decoder_stream.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "base/task_util/default_task_runner_factory.h"
#include "media/codec/codec_factory.h"
#include "media/codec/opus/opus_codec.h"

namespace ave::media {

namespace {

size_t PlaybackCapacityBytes(uint32_t sample_rate_hz,
                             uint32_t max_playback_delay_ms) {
  constexpr size_t kBytesPerMonoSample = sizeof(int16_t);
  return std::max<size_t>(1, static_cast<size_t>(sample_rate_hz) *
                                 max_playback_delay_ms / 1000 *
                                 kBytesPerMonoSample);
}

}  // namespace

AudioDecoderStream::AudioDecoderStream(const Config& config)
    : config_(config),
      pcm_ring_buffer_(PlaybackCapacityBytes(config.output_sample_rate_hz,
                                             config.max_playback_delay_ms)) {
  base::TaskRunnerFactory* task_runner_factory = config_.task_runner_factory;
  if (task_runner_factory == nullptr) {
    owned_task_runner_factory_ = base::CreateDefaultTaskRunnerFactory();
    task_runner_factory = owned_task_runner_factory_.get();
  }
  worker_runner_ =
      std::make_unique<base::TaskRunner>(task_runner_factory->CreateTaskRunner(
          "AudioDecoderStream", base::TaskRunnerFactory::Priority::HIGH));

  if (config_.decoder_config == nullptr) {
    AVE_LOG(LS_ERROR) << "AudioDecoderStream requires a decoder config";
    return;
  }
  decoder_ = CreateCodecByType(config_.codec_id, false);
  if (decoder_ == nullptr && config_.codec_id == CodecId::AVE_CODEC_ID_OPUS) {
    decoder_ = std::make_shared<OpusCodec>(false);
  }
  if (decoder_ == nullptr || decoder_->SetCallback(this) != OK ||
      decoder_->Configure(config_.decoder_config) != OK) {
    AVE_LOG(LS_ERROR) << "Failed to configure audio decoder";
    if (decoder_ != nullptr) {
      decoder_->Release();
      decoder_.reset();
    }
  }
}

AudioDecoderStream::~AudioDecoderStream() {
  Stop();
  if (decoder_ != nullptr) {
    decoder_->Release();
    decoder_.reset();
  }
  if (worker_runner_ != nullptr) {
    worker_runner_->PostTaskAndWait([]() {});
    worker_runner_.reset();
  }
}

bool AudioDecoderStream::Start() {
  if (running_.exchange(true)) {
    return true;
  }
  if (decoder_ == nullptr || decoder_->Start() != OK) {
    running_.store(false);
    return false;
  }
  return true;
}

void AudioDecoderStream::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (decoder_ != nullptr) {
    decoder_->Stop();
  }
  worker_runner_->PostTaskAndWait([this]() {
    decoder_input_indices_ = {};
    pending_frames_.clear();
    decoder_start_times_.clear();
    pcm_ring_buffer_.Clear();
    first_queued_pcm_time_us_.store(0, std::memory_order_release);
    UpdatePlaybackQueueMetric();
  });
}

void AudioDecoderStream::OnFrame(const std::shared_ptr<MediaFrame>& frame) {
  if (!running_.load() || frame == nullptr || frame->size() == 0) {
    return;
  }
  if (frame->codec() != config_.codec_id) {
    if (config_.observer != nullptr) {
      config_.observer->OnDecoderInputDrop(frame->size());
    }
    return;
  }
  const base::Timestamp received_time =
      frame->arrival_time().IsFinite()
          ? frame->arrival_time()
          : (config_.clock == nullptr ? base::Timestamp::Zero()
                                      : config_.clock->CurrentTime());
  worker_runner_->PostTask([this, frame, received_time]() {
    if (!running_.load()) {
      return;
    }
    pending_frames_.push_back({.frame = frame, .received_time = received_time});
    while (pending_frames_.size() > config_.max_pending_encoded_frames) {
      if (config_.observer != nullptr) {
        config_.observer->OnDecoderInputDrop(
            pending_frames_.front().frame->size());
      }
      pending_frames_.pop_front();
    }
    FeedDecoder();
  });
}

void AudioDecoderStream::FeedDecoder() {
  while (running_.load() && !pending_frames_.empty() &&
         !decoder_input_indices_.empty()) {
    const size_t input_index = decoder_input_indices_.front();
    decoder_input_indices_.pop();
    PendingFrame pending = std::move(pending_frames_.front());
    pending_frames_.pop_front();

    std::shared_ptr<CodecBuffer> codec_buffer;
    if (decoder_->GetInputBuffer(input_index, codec_buffer) != OK ||
        codec_buffer == nullptr || codec_buffer->data() == nullptr) {
      if (config_.observer != nullptr) {
        config_.observer->OnDecoderInputDrop(pending.frame->size());
      }
      continue;
    }
    if (pending.frame->size() > codec_buffer->capacity()) {
      if (config_.observer != nullptr) {
        config_.observer->OnDecoderInputDrop(pending.frame->size());
      }
      continue;
    }
    const size_t copy_size = pending.frame->size();
    std::memcpy(codec_buffer->data(), pending.frame->data(), copy_size);
    codec_buffer->SetRange(0, copy_size);
    const base::Timestamp now = config_.clock == nullptr
                                    ? pending.received_time
                                    : config_.clock->CurrentTime();
    decoder_start_times_.push_back(now);
    if (decoder_->QueueInputBuffer(input_index) != OK) {
      decoder_start_times_.pop_back();
      if (config_.observer != nullptr) {
        config_.observer->OnDecoderInputDrop(copy_size);
      }
    } else if (config_.observer != nullptr && now >= pending.received_time) {
      config_.observer->OnReceiveToDecoderInput(now - pending.received_time);
    }
  }
}

size_t AudioDecoderStream::ReadPcm(std::span<uint8_t> output) {
  const size_t bytes_read = pcm_ring_buffer_.Read(output);
  if (bytes_read > 0) {
    const int64_t decoded_time_us =
        first_queued_pcm_time_us_.exchange(0, std::memory_order_acq_rel);
    if (decoded_time_us > 0 && config_.clock != nullptr &&
        config_.observer != nullptr) {
      const base::Timestamp decoded_time =
          base::Timestamp::Micros(decoded_time_us);
      const base::Timestamp now = config_.clock->CurrentTime();
      if (now >= decoded_time) {
        config_.observer->OnDecodeToPlayout(now - decoded_time);
      }
    }
    UpdatePlaybackQueueMetric();
  }
  return bytes_read;
}

void AudioDecoderStream::OnInputBufferAvailable(size_t index) {
  worker_runner_->PostTask([this, index]() {
    if (!running_.load()) {
      return;
    }
    decoder_input_indices_.push(index);
    FeedDecoder();
  });
}

void AudioDecoderStream::OnOutputBufferAvailable(size_t index) {
  worker_runner_->PostTask([this, index]() { ProcessOutput(index); });
}

void AudioDecoderStream::ProcessOutput(size_t index) {
  if (decoder_ == nullptr) {
    return;
  }
  std::shared_ptr<CodecBuffer> codec_buffer;
  const status_t get_status = decoder_->GetOutputBuffer(index, codec_buffer);
  base::TimeDelta decode_duration = base::TimeDelta::Zero();
  if (!decoder_start_times_.empty()) {
    const base::Timestamp start_time = decoder_start_times_.front();
    decoder_start_times_.pop_front();
    if (config_.clock != nullptr &&
        config_.clock->CurrentTime() >= start_time) {
      decode_duration = config_.clock->CurrentTime() - start_time;
    }
  }
  if (running_.load() && get_status == OK && codec_buffer != nullptr &&
      codec_buffer->size() > 0) {
    const base::Timestamp decoded_time = config_.clock == nullptr
                                             ? base::Timestamp::Zero()
                                             : config_.clock->CurrentTime();
    const bool queue_was_empty = pcm_ring_buffer_.queued_bytes() == 0;
    const size_t written = pcm_ring_buffer_.Write(
        std::span<const uint8_t>(codec_buffer->data(), codec_buffer->size()));
    if (written == 0) {
      if (config_.observer != nullptr) {
        config_.observer->OnPlaybackPcmDrop(codec_buffer->size());
      }
    } else if (queue_was_empty && decoded_time > base::Timestamp::Zero()) {
      int64_t expected = 0;
      first_queued_pcm_time_us_.compare_exchange_strong(
          expected, decoded_time.us(), std::memory_order_acq_rel);
    }
    if (config_.observer != nullptr) {
      config_.observer->OnDecodedFrame(codec_buffer->size(), decode_duration);
    }
    UpdatePlaybackQueueMetric();
  }
  decoder_->ReleaseOutputBuffer(index, false);
}

void AudioDecoderStream::UpdatePlaybackQueueMetric() {
  if (config_.observer == nullptr || config_.output_sample_rate_hz == 0) {
    return;
  }
  constexpr int64_t kBytesPerMonoSample = sizeof(int16_t);
  const int64_t queued_duration_us =
      static_cast<int64_t>(pcm_ring_buffer_.queued_bytes()) * 1'000'000 /
      (static_cast<int64_t>(config_.output_sample_rate_hz) *
       kBytesPerMonoSample);
  config_.observer->OnPlaybackQueueDuration(
      base::TimeDelta::Micros(queued_duration_us));
}

void AudioDecoderStream::OnOutputFormatChanged(
    const std::shared_ptr<MediaMeta>& /*format*/) {}

void AudioDecoderStream::OnError(status_t error) {
  AVE_LOG(LS_ERROR) << "Audio decoder error: " << error;
  if (config_.observer != nullptr) {
    config_.observer->OnDecoderError(error);
  }
}

void AudioDecoderStream::OnFrameRendered(std::shared_ptr<Message> /*notify*/) {}

}  // namespace ave::media
