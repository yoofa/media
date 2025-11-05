/*
 * opus_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "opus_codec.h"

#include "base/logging.h"
#include "media/audio/channel_layout.h"

namespace ave {
namespace media {

namespace {
const size_t kInvalidIndex = static_cast<size_t>(-1);
const int kOpusFrameDuration = 20;  // 20ms frame duration
}  // namespace

OpusCodec::OpusCodec(bool is_encoder)
    : SimpleCodec(is_encoder),
      opus_ctx_(),
      sample_rate_(48000),
      channels_(2),
      frame_size_(0),
      complexity_(10),
      signal_type_(OPUS_AUTO) {
  opus_ctx_.encoder = nullptr;
}

OpusCodec::~OpusCodec() {
  Release();
}

status_t OpusCodec::OnConfigure(const std::shared_ptr<CodecConfig>& config) {
  auto format = config->format;
  if (format->stream_type() != MediaType::AUDIO) {
    AVE_LOG(LS_ERROR) << "OpusCodec only supports audio streams";
    return INVALID_OPERATION;
  }

  sample_rate_ = format->sample_rate();
  if (sample_rate_ <= 0) {
    sample_rate_ = 48000;  // Default to 48kHz
  }

  auto channel_layout = format->channel_layout();
  channels_ = ChannelLayoutToChannelCount(channel_layout);
  if (channels_ == 0) {
    channels_ = 2;  // Default to stereo
  }

  // Calculate frame size based on sample rate and frame duration
  frame_size_ = sample_rate_ * kOpusFrameDuration / 1000;

  AVE_LOG(LS_INFO) << "Configuring OpusCodec: sample_rate=" << sample_rate_
                   << ", channels=" << channels_
                   << ", is_encoder=" << is_encoder_;

  int32_t error = OPUS_OK;
  if (is_encoder_) {
    opus_ctx_.encoder = opus_encoder_create(
        static_cast<opus_int32>(sample_rate_), static_cast<int32_t>(channels_),
        OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK || !opus_ctx_.encoder) {
      AVE_LOG(LS_ERROR) << "Failed to create Opus encoder: " << error;
      return UNKNOWN_ERROR;
    }

    // Set encoder parameters
    auto bitrate = format->bitrate();
    if (bitrate > 0) {
      opus_encoder_ctl(opus_ctx_.encoder, OPUS_SET_BITRATE(bitrate));
    }
    opus_encoder_ctl(opus_ctx_.encoder, OPUS_SET_COMPLEXITY(complexity_));
    opus_encoder_ctl(opus_ctx_.encoder, OPUS_SET_SIGNAL(signal_type_));
  } else {
    opus_ctx_.decoder =
        opus_decoder_create(static_cast<opus_int32>(sample_rate_),
                            static_cast<int32_t>(channels_), &error);
    opus_ctx_.decoder =
        opus_decoder_create(static_cast<opus_int32>(sample_rate_),
                            static_cast<int32_t>(channels_), &error);
    if (error != OPUS_OK || !opus_ctx_.decoder) {
      AVE_LOG(LS_ERROR) << "Failed to create Opus decoder: " << error
                        << ", sample_rate=" << sample_rate_
                        << ", channels=" << channels_;
      return UNKNOWN_ERROR;
    }
  }

  return OK;
}

status_t OpusCodec::OnStart() {
  return OK;
}

status_t OpusCodec::OnStop() {
  return OK;
}

status_t OpusCodec::OnReset() {
  if (is_encoder_ && opus_ctx_.encoder) {
    opus_encoder_ctl(opus_ctx_.encoder, OPUS_RESET_STATE);
  } else if (!is_encoder_ && opus_ctx_.decoder) {
    opus_decoder_ctl(opus_ctx_.decoder, OPUS_RESET_STATE);
  }
  return OK;
}

status_t OpusCodec::OnFlush() {
  if (is_encoder_ && opus_ctx_.encoder) {
    opus_encoder_ctl(opus_ctx_.encoder, OPUS_RESET_STATE);
  } else if (!is_encoder_ && opus_ctx_.decoder) {
    opus_decoder_ctl(opus_ctx_.decoder, OPUS_RESET_STATE);
  }
  return OK;
}

status_t OpusCodec::OnRelease() {
  if (is_encoder_ && opus_ctx_.encoder) {
    opus_encoder_destroy(opus_ctx_.encoder);
    opus_ctx_.encoder = nullptr;
  } else if (!is_encoder_ && opus_ctx_.decoder) {
    opus_decoder_destroy(opus_ctx_.decoder);
    opus_ctx_.decoder = nullptr;
  }
  return OK;
}

void OpusCodec::ProcessInput(size_t index) {
  std::lock_guard<std::mutex> lock(lock_);
  if (index >= input_buffers_.size() || !input_buffers_[index].in_use) {
    AVE_LOG(LS_WARNING) << "Invalid input buffer index or not in use";
    return;
  }

  auto& buffer = input_buffers_[index].buffer;

  if (is_encoder_) {
    // Encoding: PCM to Opus
    const auto* pcm = reinterpret_cast<const opus_int16*>(buffer->data());
    int pcm_samples = buffer->size() / (channels_ * sizeof(opus_int16));

    // Opus requires exact frame size
    if (pcm_samples != frame_size_) {
      if (pcm_samples < frame_size_ && pcm_samples > 0) {
        AVE_LOG(LS_INFO) << "Padding partial frame: " << pcm_samples << " -> "
                         << frame_size_;
        // Pad with zeros
        size_t current_size = buffer->size();
        size_t required_size = frame_size_ * channels_ * sizeof(opus_int16);
        if (buffer->capacity() >= required_size) {
          std::memset(buffer->data() + current_size, 0,
                      required_size - current_size);
          buffer->SetRange(0, required_size);
          pcm_samples = frame_size_;
        } else {
          AVE_LOG(LS_WARNING) << "Buffer capacity insufficient for padding";
          input_buffers_[index].in_use = false;
          NotifyInputBufferAvailable(index);
          return;
        }
      } else {
        AVE_LOG(LS_WARNING)
            << "PCM samples (" << pcm_samples << ") doesn't match frame size ("
            << frame_size_ << "), skipping frame";
        input_buffers_[index].in_use = false;
        NotifyInputBufferAvailable(index);
        return;
      }
    }

    size_t output_index = GetAvailableOutputBufferIndex();
    if (output_index == kInvalidIndex) {
      AVE_LOG(LS_WARNING) << "No available output buffer for encoding";
      input_buffers_[index].in_use = false;
      NotifyInputBufferAvailable(index);
      return;
    }

    auto& output_buffer = output_buffers_[output_index].buffer;
    int encoded_bytes =
        opus_encode(opus_ctx_.encoder, pcm, frame_size_, output_buffer->data(),
                    static_cast<opus_int32>(output_buffer->capacity()));

    if (encoded_bytes < 0) {
      AVE_LOG(LS_ERROR) << "Opus encoding failed: " << encoded_bytes;
      input_buffers_[index].in_use = false;
      NotifyInputBufferAvailable(index);
      NotifyError(UNKNOWN_ERROR);
      return;
    }

    output_buffer->SetRange(0, encoded_bytes);
    AVE_LOG(LS_INFO) << "Encoded " << pcm_samples << " samples to "
                     << encoded_bytes << " bytes";

    size_t pushed_index = PushOutputBuffer(output_index);
    if (pushed_index != kInvalidIndex) {
      NotifyOutputBufferAvailable(pushed_index);
    }
  } else {
    // Decoding: Opus to PCM
    const unsigned char* opus_data = buffer->data();
    uint32_t opus_size = buffer->size();
    bool is_plc = (opus_size == 0);

    size_t output_index = GetAvailableOutputBufferIndex();
    if (output_index == kInvalidIndex) {
      AVE_LOG(LS_WARNING) << "No available output buffer for decoding";
      input_buffers_[index].in_use = false;
      NotifyInputBufferAvailable(index);
      return;
    }

    auto& output_buffer = output_buffers_[output_index].buffer;
    auto* pcm = reinterpret_cast<opus_int16*>(output_buffer->data());
    auto max_samples =
        output_buffer->capacity() / (channels_ * sizeof(opus_int16));

    int decoded_samples = 0;
    if (is_plc) {
      // Treat empty buffer as EOS for now to avoid crash/infinite loop
      decoded_samples = 0;
    } else {
      decoded_samples = opus_decode(opus_ctx_.decoder, opus_data,
                                    static_cast<opus_int32>(opus_size), pcm,
                                    static_cast<int32_t>(max_samples), 0);
    }

    if (decoded_samples < 0) {
      AVE_LOG(LS_ERROR) << "Opus decoding failed: " << decoded_samples;
      input_buffers_[index].in_use = false;
      NotifyInputBufferAvailable(index);
      NotifyError(UNKNOWN_ERROR);
      return;
    }

    auto pcm_bytes = decoded_samples * channels_ * sizeof(opus_int16);
    output_buffer->SetRange(0, pcm_bytes);
    if (!is_plc) {
      AVE_LOG(LS_INFO) << "Decoded " << opus_size << " bytes to "
                       << decoded_samples << " samples (" << pcm_bytes
                       << " bytes)";
    }

    size_t pushed_index = PushOutputBuffer(output_index);
    if (pushed_index != kInvalidIndex) {
      NotifyOutputBufferAvailable(pushed_index);
    }
  }

  input_buffers_[index].in_use = false;
  NotifyInputBufferAvailable(index);
}

void OpusCodec::ProcessOutput() {
  // OpusCodec generates output synchronously in ProcessInput, so no need to
  // generate output here.
}

}  // namespace media
}  // namespace ave
