/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/audio_encoder_stream.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "base/task_util/default_task_runner_factory.h"
#include "media/codec/codec_factory.h"
#include "media/codec/opus/opus_codec.h"

namespace ave::media {

AudioEncoderStream::AudioEncoderStream(const Config& config)
    : AsyncMediaFrameSource(config.task_runner_factory), config_(config) {
  base::TaskRunnerFactory* task_runner_factory = config_.task_runner_factory;
  if (task_runner_factory == nullptr) {
    owned_task_runner_factory_ = base::CreateDefaultTaskRunnerFactory();
    task_runner_factory = owned_task_runner_factory_.get();
  }
  worker_runner_ =
      std::make_unique<base::TaskRunner>(task_runner_factory->CreateTaskRunner(
          "AudioEncoderStream", base::TaskRunnerFactory::Priority::HIGH));

  if (config_.encoder_config == nullptr) {
    AVE_LOG(LS_ERROR) << "AudioEncoderStream requires an encoder config";
    return;
  }
  encoder_ = CreateCodecByType(config_.codec_id, true);
  if (encoder_ == nullptr && config_.codec_id == CodecId::AVE_CODEC_ID_OPUS) {
    encoder_ = std::make_shared<OpusCodec>(true);
  }
  if (encoder_ == nullptr) {
    AVE_LOG(LS_ERROR) << "Failed to create audio encoder";
    return;
  }
  if (encoder_->SetCallback(this) != OK ||
      encoder_->Configure(config_.encoder_config) != OK) {
    AVE_LOG(LS_ERROR) << "Failed to configure audio encoder";
    encoder_->Release();
    encoder_.reset();
  }
}

AudioEncoderStream::~AudioEncoderStream() {
  Stop();
  if (encoder_ != nullptr) {
    encoder_->Release();
    encoder_.reset();
  }
  if (worker_runner_ != nullptr) {
    worker_runner_->PostTaskAndWait([]() {});
    worker_runner_.reset();
  }
}

bool AudioEncoderStream::Start() {
  if (running_.exchange(true)) {
    return true;
  }
  if (encoder_ == nullptr || encoder_->Start() != OK) {
    running_.store(false);
    return false;
  }
  return true;
}

void AudioEncoderStream::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (encoder_ != nullptr) {
    encoder_->Stop();
  }
  worker_runner_->PostTaskAndWait([this]() {
    encoder_input_indices_ = {};
    pcm_buffer_.clear();
    pcm_chunk_timings_.clear();
    encoded_frame_timings_.clear();
    next_pts_.reset();
  });
}

void AudioEncoderStream::DeliverPcm(std::span<const uint8_t> pcm) {
  if (!running_.load() || pcm.empty()) {
    return;
  }
  std::vector<uint8_t> copy(pcm.begin(), pcm.end());
  const base::Timestamp arrival_time = config_.clock == nullptr
                                           ? base::Timestamp::Zero()
                                           : config_.clock->CurrentTime();
  worker_runner_->PostTask(
      [this, copy = std::move(copy), arrival_time]() mutable {
        if (!running_.load()) {
          return;
        }
        const size_t bytes = copy.size();
        pcm_buffer_.insert(pcm_buffer_.end(), copy.begin(), copy.end());
        pcm_chunk_timings_.push_back(
            {.remaining_bytes = bytes, .arrival_time = arrival_time});
        FeedEncoder();
      });
}

void AudioEncoderStream::FeedEncoder() {
  const size_t samples_per_frame = static_cast<size_t>(config_.sample_rate_hz) *
                                   config_.frame_duration_ms / 1000;
  const size_t frame_size = samples_per_frame * sizeof(int16_t);
  while (frame_size > 0 && pcm_buffer_.size() >= frame_size &&
         !encoder_input_indices_.empty() && running_.load()) {
    const size_t input_index = encoder_input_indices_.front();
    encoder_input_indices_.pop();

    std::shared_ptr<CodecBuffer> codec_buffer;
    if (encoder_->GetInputBuffer(input_index, codec_buffer) != OK ||
        codec_buffer == nullptr || codec_buffer->data() == nullptr ||
        codec_buffer->capacity() < frame_size) {
      continue;
    }

    std::memcpy(codec_buffer->data(), pcm_buffer_.data(), frame_size);
    codec_buffer->SetRange(0, frame_size);
    const base::Timestamp capture_time = ConsumePcmTiming(frame_size);
    const base::Timestamp now =
        config_.clock == nullptr ? capture_time : config_.clock->CurrentTime();
    if (!next_pts_.has_value()) {
      next_pts_ = capture_time;
    }
    const EncodedFrameTiming timing{
        .pts = *next_pts_,
        .encode_start = now,
    };
    next_pts_ = *next_pts_ + base::TimeDelta::Millis(config_.frame_duration_ms);
    encoded_frame_timings_.push_back(timing);
    if (encoder_->QueueInputBuffer(input_index) != OK) {
      encoded_frame_timings_.pop_back();
    } else if (config_.observer != nullptr && now >= capture_time) {
      config_.observer->OnCaptureToEncoderInput(now - capture_time);
    }
    pcm_buffer_.erase(
        pcm_buffer_.begin(),
        pcm_buffer_.begin() + static_cast<std::ptrdiff_t>(frame_size));
  }
}

base::Timestamp AudioEncoderStream::ConsumePcmTiming(size_t bytes) {
  const base::Timestamp first_arrival =
      pcm_chunk_timings_.empty() ? base::Timestamp::Zero()
                                 : pcm_chunk_timings_.front().arrival_time;
  size_t remaining = bytes;
  while (remaining > 0 && !pcm_chunk_timings_.empty()) {
    PcmChunkTiming& chunk = pcm_chunk_timings_.front();
    const size_t consumed = std::min(remaining, chunk.remaining_bytes);
    remaining -= consumed;
    chunk.remaining_bytes -= consumed;
    if (chunk.remaining_bytes == 0) {
      pcm_chunk_timings_.pop_front();
    }
  }
  return first_arrival;
}

void AudioEncoderStream::OnInputBufferAvailable(size_t index) {
  worker_runner_->PostTask([this, index]() {
    if (!running_.load()) {
      return;
    }
    encoder_input_indices_.push(index);
    FeedEncoder();
  });
}

void AudioEncoderStream::OnOutputBufferAvailable(size_t index) {
  worker_runner_->PostTask([this, index]() { ProcessOutput(index); });
}

void AudioEncoderStream::ProcessOutput(size_t index) {
  if (encoder_ == nullptr) {
    return;
  }
  std::shared_ptr<CodecBuffer> codec_buffer;
  const status_t get_status = encoder_->GetOutputBuffer(index, codec_buffer);
  EncodedFrameTiming timing;
  bool has_timing = false;
  if (!encoded_frame_timings_.empty()) {
    timing = encoded_frame_timings_.front();
    encoded_frame_timings_.pop_front();
    has_timing = true;
  }
  if (running_.load() && get_status == OK && codec_buffer != nullptr &&
      codec_buffer->size() > 0 && has_timing) {
    const base::Timestamp now = config_.clock == nullptr
                                    ? timing.encode_start
                                    : config_.clock->CurrentTime();
    const base::TimeDelta encode_duration = now >= timing.encode_start
                                                ? now - timing.encode_start
                                                : base::TimeDelta::Zero();
    if (config_.observer != nullptr) {
      config_.observer->OnEncodedFrame(codec_buffer->size(), encode_duration);
    }
    DeliverFrame(CreateEncodedFrame(*codec_buffer, timing));
  }
  encoder_->ReleaseOutputBuffer(index, false);
}

std::shared_ptr<MediaFrame> AudioEncoderStream::CreateEncodedFrame(
    CodecBuffer& codec_buffer,
    const EncodedFrameTiming& timing) const {
  auto frame = MediaFrame::CreateShared(codec_buffer.size(), MediaType::AUDIO);
  std::memcpy(frame->data(), codec_buffer.data(), codec_buffer.size());
  frame->setRange(0, codec_buffer.size());
  frame->SetCodec(config_.codec_id)
      .SetSampleRate(config_.sample_rate_hz)
      .SetChannelLayout(config_.channel_layout)
      .SetSamplesPerChannel(static_cast<int64_t>(config_.sample_rate_hz) *
                            config_.frame_duration_ms / 1000)
      .SetPts(timing.pts)
      .SetDts(timing.pts)
      .SetDuration(base::TimeDelta::Millis(config_.frame_duration_ms));

  const std::shared_ptr<MediaMeta>& format =
      output_format_ != nullptr ? output_format_
                                : config_.encoder_config->format;
  if (format != nullptr) {
    const std::shared_ptr<base::Buffer> private_data = format->private_data();
    if (private_data != nullptr && !private_data->empty()) {
      frame->SetPrivateData(static_cast<uint32_t>(private_data->size()),
                            private_data->data());
    }
  }
  return frame;
}

void AudioEncoderStream::OnOutputFormatChanged(
    const std::shared_ptr<MediaMeta>& format) {
  worker_runner_->PostTask([this, format]() { output_format_ = format; });
}

void AudioEncoderStream::OnError(status_t error) {
  AVE_LOG(LS_ERROR) << "Audio encoder error: " << error;
  if (config_.observer != nullptr) {
    config_.observer->OnEncoderError(error);
  }
}

void AudioEncoderStream::OnFrameRendered(std::shared_ptr<Message> /*notify*/) {}

}  // namespace ave::media
