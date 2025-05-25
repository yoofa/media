/*
 * aaudio_audio_record.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "aaudio_audio_record.h"

#include <cmath>

#include "base/logging.h"
#include "media/audio/channel_layout.h"

namespace ave {
namespace media {
namespace android {

namespace {

aaudio_format_t ToAAudioFormat(AudioFormat format) {
  switch (format) {
    case AudioFormat::AUDIO_FORMAT_PCM_16_BIT:
      return AAUDIO_FORMAT_PCM_I16;
    case AudioFormat::AUDIO_FORMAT_PCM_FLOAT:
      return AAUDIO_FORMAT_PCM_FLOAT;
    case AudioFormat::AUDIO_FORMAT_PCM_32_BIT:
      return AAUDIO_FORMAT_PCM_I32;
    default:
      return AAUDIO_FORMAT_UNSPECIFIED;
  }
}

}  // namespace

AAudioAudioRecord::AAudioAudioRecord() = default;

AAudioAudioRecord::~AAudioAudioRecord() {
  Close();
}

bool AAudioAudioRecord::ready() const {
  return stream_ != nullptr;
}

ssize_t AAudioAudioRecord::bufferSize() const {
  if (!stream_) {
    return 0;
  }
  int32_t frames = AAudioStream_getBufferSizeInFrames(stream_);
  return frames > 0 ? frames * frameSize() : 0;
}

ssize_t AAudioAudioRecord::frameCount() const {
  if (!stream_) {
    return 0;
  }
  int32_t frames = AAudioStream_getBufferSizeInFrames(stream_);
  return frames > 0 ? frames : 0;
}

ssize_t AAudioAudioRecord::channelCount() const {
  return ChannelLayoutToChannelCount(config_.channel_layout);
}

ssize_t AAudioAudioRecord::frameSize() const {
  int bytes_per_sample = 2;
  switch (config_.format) {
    case AudioFormat::AUDIO_FORMAT_PCM_8_BIT:
      bytes_per_sample = 1;
      break;
    case AudioFormat::AUDIO_FORMAT_PCM_16_BIT:
      bytes_per_sample = 2;
      break;
    case AudioFormat::AUDIO_FORMAT_PCM_32_BIT:
    case AudioFormat::AUDIO_FORMAT_PCM_FLOAT:
      bytes_per_sample = 4;
      break;
    default:
      bytes_per_sample = 2;
      break;
  }
  return ChannelLayoutToChannelCount(config_.channel_layout) * bytes_per_sample;
}

uint32_t AAudioAudioRecord::sampleRate() const {
  return config_.sample_rate;
}

uint32_t AAudioAudioRecord::latency() const {
  if (!stream_) {
    return 0;
  }
  int32_t burst_frames = AAudioStream_getFramesPerBurst(stream_);
  if (burst_frames <= 0) {
    return 0;
  }
  return static_cast<uint32_t>((static_cast<int64_t>(burst_frames) * 1000) /
                               config_.sample_rate);
}

status_t AAudioAudioRecord::GetPosition(uint32_t* position) const {
  if (!stream_ || !position) {
    return -EINVAL;
  }
  int64_t frames = AAudioStream_getFramesRead(stream_);
  if (frames < 0) {
    return -EINVAL;
  }
  *position = static_cast<uint32_t>(frames);
  return 0;
}

int64_t AAudioAudioRecord::GetRecordedDurationUs(int64_t nowUs) const {
  (void)nowUs;
  return 0;
}

status_t AAudioAudioRecord::GetFramesRead(uint32_t* frames_read) const {
  if (!stream_ || !frames_read) {
    return -EINVAL;
  }
  int64_t frames = AAudioStream_getFramesRead(stream_);
  if (frames < 0) {
    return -EINVAL;
  }
  *frames_read = static_cast<uint32_t>(frames);
  return 0;
}

status_t AAudioAudioRecord::Open(audio_config_t config,
                                 AudioCallback cb,
                                 void* cookie) {
  if (stream_) {
    return -EEXIST;
  }
  config_ = config;
  callback_ = cb;
  cookie_ = cookie;

  AAudioStreamBuilder* builder = nullptr;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK || !builder) {
    AVE_LOG(LS_ERROR) << "AAudio_createStreamBuilder failed: " << result;
    return INVALID_OPERATION;
  }

  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
  AAudioStreamBuilder_setSampleRate(builder,
                                    static_cast<int32_t>(config.sample_rate));
  AAudioStreamBuilder_setChannelCount(
      builder, ChannelLayoutToChannelCount(config.channel_layout));
  AAudioStreamBuilder_setFormat(builder, ToAAudioFormat(config.format));
  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);

  if (callback_) {
    AAudioStreamBuilder_setDataCallback(builder, DataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, ErrorCallback, this);
  }

  result = AAudioStreamBuilder_openStream(builder, &stream_);
  AAudioStreamBuilder_delete(builder);
  if (result != AAUDIO_OK || !stream_) {
    AVE_LOG(LS_ERROR) << "Failed to open AAudio record stream: " << result;
    stream_ = nullptr;
    return INVALID_OPERATION;
  }

  return 0;
}

status_t AAudioAudioRecord::Start() {
  if (!stream_) {
    return -EINVAL;
  }
  aaudio_result_t result = AAudioStream_requestStart(stream_);
  if (result != AAUDIO_OK) {
    AVE_LOG(LS_ERROR) << "AAudioStream_requestStart failed: " << result;
    return INVALID_OPERATION;
  }
  started_ = true;
  return 0;
}

void AAudioAudioRecord::Stop() {
  if (!stream_) {
    return;
  }
  (void)AAudioStream_requestStop(stream_);
  started_ = false;
}

void AAudioAudioRecord::Flush() {
  // No explicit flush for input
}

void AAudioAudioRecord::Pause() {
  if (!stream_) {
    return;
  }
  (void)AAudioStream_requestPause(stream_);
  started_ = false;
}

void AAudioAudioRecord::Close() {
  if (!stream_) {
    return;
  }
  Stop();
  AAudioStream_close(stream_);
  stream_ = nullptr;
}

ssize_t AAudioAudioRecord::Read(void* buffer, size_t size, bool blocking) {
  if (!stream_ || callback_) {
    return -EINVAL;  // callback mode doesn't use Read
  }
  auto frames = static_cast<int32_t>(size / frameSize());
  if (frames <= 0) {
    return 0;
  }

  // unified read logic using do-while
  int32_t total_read = 0;
  int32_t remaining_frames = frames;
  char* buffer_ptr = static_cast<char*>(buffer);

  do {
    int32_t read = AAudioStream_read(stream_, buffer_ptr, remaining_frames, 0);
    if (read < 0) {
      // Error occurred
      return read;
    }
    {
      // Some frames read successfully
      total_read += read;
      remaining_frames -= read;
      buffer_ptr += read * frameSize();
    }
  } while (blocking && remaining_frames > 0);

  if (total_read <= 0) {
    return total_read;
  }

  // Apply simple gain if not 1.0
  if (std::fabs(gain_ - 1.0f) > 1e-6) {
    if (config_.format == AudioFormat::AUDIO_FORMAT_PCM_16_BIT) {
      auto* samples = static_cast<int16_t*>(buffer);
      size_t count = static_cast<size_t>(total_read) * channelCount();
      for (size_t i = 0; i < count; ++i) {
        auto v = static_cast<int32_t>(static_cast<float>(samples[i]) * gain_);
        if (v > 32767) {
          v = 32767;
        }
        if (v < -32768) {
          v = -32768;
        }
        samples[i] = static_cast<int16_t>(v);
      }
    }
  }

  return static_cast<ssize_t>(total_read * frameSize());
}

status_t AAudioAudioRecord::SetGain(float gain) {
  gain_ = gain;
  return 0;
}

float AAudioAudioRecord::GetGain() const {
  return gain_;
}

aaudio_data_callback_result_t AAudioAudioRecord::DataCallback(
    AAudioStream* stream AVE_MAYBE_UNUSED,
    void* userData,
    void* audioData,
    int32_t numFrames) {
  auto* self = static_cast<AAudioAudioRecord*>(userData);
  if (!self || !self->callback_) {
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }
  size_t bytes = static_cast<size_t>(numFrames) * self->frameSize();
  self->callback_(self, audioData, bytes, self->cookie_, CB_EVENT_MORE_DATA);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioAudioRecord::ErrorCallback(AAudioStream* stream AVE_MAYBE_UNUSED,
                                      void* userData AVE_MAYBE_UNUSED,
                                      aaudio_result_t error) {
  AVE_LOG(LS_ERROR) << "AAudio record stream error: " << error;
}

}  // namespace android
}  // namespace media
}  // namespace ave
