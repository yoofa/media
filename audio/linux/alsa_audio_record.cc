/*
 * alsa_audio_record.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "alsa_audio_record.h"

#include <algorithm>

#include "base/logging.h"
#include "media/audio/channel_layout.h"
#include "media/audio/linux/alsa_symbol_table.h"

#define LATE(sym) LATESYM_GET(AlsaSymbolTable, symbol_table_, sym)

namespace ave {
namespace media {
namespace linux_audio {

namespace {
constexpr size_t kDefaultPeriodSizeMs = 10;  // 10ms
constexpr size_t kDefaultBufferCount = 4;    // 4 period
}  // namespace

AlsaAudioRecord::AlsaAudioRecord(AlsaSymbolTable* symbol_table)
    : symbol_table_(symbol_table),
      handle_(nullptr),
      callback_(nullptr),
      cookie_(nullptr),
      ready_(false),
      recording_(false),
      period_size_(0),
      buffer_size_(0),
      frames_read_(0),
      gain_(1.0f) {}

AlsaAudioRecord::~AlsaAudioRecord() {
  Close();
}

bool AlsaAudioRecord::ready() const {
  return ready_;
}

ssize_t AlsaAudioRecord::bufferSize() const {
  return static_cast<ssize_t>(buffer_size_) * frameSize();
}

ssize_t AlsaAudioRecord::frameCount() const {
  return static_cast<ssize_t>(buffer_size_);
}

ssize_t AlsaAudioRecord::channelCount() const {
  return ChannelLayoutToChannelCount(config_.channel_layout);
}

ssize_t AlsaAudioRecord::frameSize() const {
  // For PCM_16_BIT format, we use 2 bytes per sample
  return ChannelLayoutToChannelCount(config_.channel_layout) * 2;
}

uint32_t AlsaAudioRecord::sampleRate() const {
  return config_.sample_rate;
}

uint32_t AlsaAudioRecord::latency() const {
  if (!ready_) {
    return 0;
  }
  snd_pcm_sframes_t delay = 0;
  if (LATE(snd_pcm_delay)(handle_, &delay) < 0) {
    return 0;
  }
  return delay * 1000 / config_.sample_rate;
}

status_t AlsaAudioRecord::GetPosition(uint32_t* position) const {
  if (!ready_ || !position) {
    return -EINVAL;
  }
  snd_pcm_sframes_t delay{};
  if (LATE(snd_pcm_delay)(handle_, &delay) < 0) {
    return -EINVAL;
  }
  *position = static_cast<uint32_t>(frames_read_);
  return 0;
}

int64_t AlsaAudioRecord::GetRecordedDurationUs(int64_t nowUs) const {
  return nowUs;
}

status_t AlsaAudioRecord::GetFramesRead(uint32_t* frames_read) const {
  if (!frames_read) {
    return -EINVAL;
  }
  *frames_read = static_cast<uint32_t>(frames_read_);
  return 0;
}

status_t AlsaAudioRecord::Open(audio_config_t config,
                               AudioCallback cb,
                               void* cookie) {
  if (ready_) {
    return -EEXIST;
  }
  config_ = config;
  callback_ = cb;
  cookie_ = cookie;

  // Open PCM device for capture
  int err = LATE(snd_pcm_open)(&handle_, "default", SND_PCM_STREAM_CAPTURE,
                               SND_PCM_NONBLOCK);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot open capture device: "
                      << LATE(snd_strerror)(err);
    return err;
  }

  err = SetHWParams();
  if (err < 0) {
    Close();
    return err;
  }

  err = SetSWParams();
  if (err < 0) {
    Close();
    return err;
  }

  ready_ = true;
  return 0;
}

status_t AlsaAudioRecord::SetHWParams() {
  snd_pcm_hw_params_t* params{};
  LATE(snd_pcm_hw_params_malloc)(&params);

  int err = LATE(snd_pcm_hw_params_any)(handle_, params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot get hw params: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  err = LATE(snd_pcm_hw_params_set_access)(handle_, params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set access type: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  err = LATE(snd_pcm_hw_params_set_format)(handle_, params,
                                           SND_PCM_FORMAT_S16_LE);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set format: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  err = LATE(snd_pcm_hw_params_set_channels)(
      handle_, params, ChannelLayoutToChannelCount(config_.channel_layout));
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set channels: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  unsigned int rate = config_.sample_rate;
  err = LATE(snd_pcm_hw_params_set_rate_near)(handle_, params, &rate, nullptr);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set rate: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  period_size_ = rate * kDefaultPeriodSizeMs / 1000;
  err = LATE(snd_pcm_hw_params_set_period_size_near)(handle_, params,
                                                     &period_size_, nullptr);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set period size: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  buffer_size_ = period_size_ * kDefaultBufferCount;
  err = LATE(snd_pcm_hw_params_set_buffer_size_near)(handle_, params,
                                                     &buffer_size_);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set buffer size: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  err = LATE(snd_pcm_hw_params)(handle_, params);
  LATE(snd_pcm_hw_params_free)(params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set hw params: " << LATE(snd_strerror)(err);
    return err;
  }
  return 0;
}

status_t AlsaAudioRecord::SetSWParams() {
  snd_pcm_sw_params_t* params{};
  LATE(snd_pcm_sw_params_malloc)(&params);
  int err = LATE(snd_pcm_sw_params_current)(handle_, params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot get sw params: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_sw_params_free)(params);
    return err;
  }
  // Start when half buffer filled (for capture this is less critical)
  err = LATE(snd_pcm_sw_params_set_start_threshold)(handle_, params,
                                                    buffer_size_ / 2);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set start threshold: "
                      << LATE(snd_strerror)(err);
    LATE(snd_pcm_sw_params_free)(params);
    return err;
  }

  err = LATE(snd_pcm_sw_params)(handle_, params);
  LATE(snd_pcm_sw_params_free)(params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set sw params: " << LATE(snd_strerror)(err);
    return err;
  }
  return 0;
}

status_t AlsaAudioRecord::RecoverIfNeeded(int error) {
  if (error == -EPIPE) {  // Overrun
    error = LATE(snd_pcm_prepare)(handle_);
    if (error < 0) {
      AVE_LOG(LS_ERROR) << "Cannot recover from overrun: "
                        << LATE(snd_strerror)(error);
      return error;
    }
  } else if (error == -ESTRPIPE) {  // Suspended
    while ((error = LATE(snd_pcm_resume)(handle_)) == -EAGAIN) {
      usleep(100000);
    }
    if (error < 0) {
      error = LATE(snd_pcm_prepare)(handle_);
      if (error < 0) {
        AVE_LOG(LS_ERROR) << "Cannot recover from suspend: "
                          << LATE(snd_strerror)(error);
        return error;
      }
    }
  }
  return 0;
}

ssize_t AlsaAudioRecord::Read(void* buffer, size_t size, bool blocking) {
  if (!ready_ || !recording_) {
    return -EINVAL;
  }
  auto* out = static_cast<uint8_t*>(buffer);
  size_t frames = size / frameSize();
  ssize_t total = 0;

  while (frames > 0) {
    snd_pcm_sframes_t ret = LATE(snd_pcm_readi)(handle_, out, frames);
    if (ret == -EAGAIN) {
      if (!blocking) {
        break;
      }
      LATE(snd_pcm_wait)(handle_, -1);
      continue;
    }
    if (ret < 0) {
      if (ret == -EPIPE) {
        LATE(snd_pcm_prepare)(handle_);
        continue;
      }
      return ret;
    }
    frames -= ret;
    out += ret * frameSize();
    total += ret;
    frames_read_ += ret;
  }

  return total * frameSize();
}

status_t AlsaAudioRecord::Start() {
  if (!ready_) {
    return -EINVAL;
  }
  if (recording_) {
    return 0;
  }

  int err = LATE(snd_pcm_prepare)(handle_);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot prepare capture: " << LATE(snd_strerror)(err);
    return err;
  }
  err = LATE(snd_pcm_start)(handle_);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot start capture: " << LATE(snd_strerror)(err);
    return err;
  }
  recording_ = true;
  return 0;
}

void AlsaAudioRecord::Stop() {
  if (!ready_ || !recording_) {
    return;
  }
  LATE(snd_pcm_drop)(handle_);
  recording_ = false;
}

void AlsaAudioRecord::Flush() {
  if (!ready_) {
    return;
  }
  LATE(snd_pcm_drop)(handle_);
  LATE(snd_pcm_prepare)(handle_);
}

void AlsaAudioRecord::Pause() {
  if (!ready_ || !recording_) {
    return;
  }
  LATE(snd_pcm_pause)(handle_, 1);
  recording_ = false;
}

void AlsaAudioRecord::Close() {
  if (handle_) {
    Stop();
    LATE(snd_pcm_close)(handle_);
    handle_ = nullptr;
  }
  ready_ = false;
  recording_ = false;
}

status_t AlsaAudioRecord::SetGain(float gain) {
  gain_ = gain;
  return 0;
}

float AlsaAudioRecord::GetGain() const {
  return gain_;
}

}  // namespace linux_audio
}  // namespace media
}  // namespace ave
