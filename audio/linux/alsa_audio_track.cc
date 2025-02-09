/*
 * alsa_audio_track.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/alsa_audio_track.h"

#include <algorithm>

#include "base/logging.h"
#include "media/audio/linux/alsa_symbol_table.h"

#define LATE(sym)                                                             \
  LATESYM_GET(ave::media::linux_audio::AlsaSymbolTable, GetAlsaSymbolTable(), \
              sym)

namespace ave {
namespace media {
namespace linux_audio {

namespace {
// 默认的ALSA buffer参数
constexpr size_t kDefaultPeriodSizeMs = 10;  // 10ms
constexpr size_t kDefaultBufferCount = 4;    // 4个period
}  // namespace

AlsaAudioTrack::AlsaAudioTrack()
    : handle_(nullptr),
      callback_(nullptr),
      cookie_(nullptr),
      ready_(false),
      playing_(false),
      buffer_size_(0),
      period_size_(0),
      temp_buffer_(nullptr),
      temp_buffer_size_(0) {}

AlsaAudioTrack::~AlsaAudioTrack() {
  Close();
}

bool AlsaAudioTrack::ready() const {
  return ready_;
}

ssize_t AlsaAudioTrack::bufferSize() const {
  return buffer_size_ * frameSize();
}

ssize_t AlsaAudioTrack::frameCount() const {
  return buffer_size_;
}

ssize_t AlsaAudioTrack::channelCount() const {
  return config_.channel_count;
}

ssize_t AlsaAudioTrack::frameSize() const {
  return config_.channel_count *
         (snd_pcm_format_physical_width(SND_PCM_FORMAT_S16_LE) / 8);
}

uint32_t AlsaAudioTrack::sampleRate() const {
  return config_.sample_rate;
}

uint32_t AlsaAudioTrack::latency() const {
  if (!ready_)
    return 0;

  snd_pcm_sframes_t delay = 0;
  if (snd_pcm_delay(handle_, &delay) < 0) {
    return 0;
  }
  return delay * 1000 / config_.sample_rate;
}

float AlsaAudioTrack::msecsPerFrame() const {
  return 1000.0f / config_.sample_rate;
}

status_t AlsaAudioTrack::GetPosition(uint32_t* position) const {
  if (!ready_ || !position)
    return -EINVAL;

  snd_pcm_sframes_t delay;
  snd_pcm_sframes_t avail;

  if (snd_pcm_delay(handle_, &delay) < 0) {
    return -EINVAL;
  }

  if (snd_pcm_avail(handle_, &avail) < 0) {
    return -EINVAL;
  }

  *position = (buffer_size_ - (delay + avail)) % buffer_size_;
  return 0;
}

status_t AlsaAudioTrack::Open(audio_config_t config,
                              AudioCallback cb,
                              void* cookie) {
  if (ready_)
    return -EEXIST;

  config_ = config;
  callback_ = cb;
  cookie_ = cookie;

  status_t res = InitDevice();
  if (res != 0) {
    Close();
    return res;
  }

  ready_ = true;
  return 0;
}

status_t AlsaAudioTrack::InitDevice() {
  // 加载ALSA符号表
  if (!GetAlsaSymbolTable()->Load()) {
    LOG(ERROR) << "Failed to load ALSA symbols";
    return -ENODEV;
  }

  int err;

  // Open PCM device
  err = snd_pcm_open(&handle_, "default", SND_PCM_STREAM_PLAYBACK,
                     SND_PCM_NONBLOCK);
  if (err < 0) {
    LOG(ERROR) << "Cannot open audio device: " << snd_strerror(err);
    return err;
  }

  // Set HW params
  err = SetHWParams();
  if (err < 0) {
    return err;
  }

  // Set SW params
  err = SetSWParams();
  if (err < 0) {
    return err;
  }

  // Allocate temp buffer
  temp_buffer_size_ = period_size_ * frameSize();
  temp_buffer_ = std::make_unique<uint8_t[]>(temp_buffer_size_);

  return 0;
}

status_t AlsaAudioTrack::SetHWParams() {
  snd_pcm_hw_params_t* params;
  snd_pcm_hw_params_alloca(&params);

  // 获取完整的参数空间
  int err = LATE(snd_pcm_hw_params_any)(handle_, params);
  if (err < 0) {
    LOG(ERROR) << "Cannot get hw params: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置访问类型为交错模式
  err = LATE(snd_pcm_hw_params_set_access)(handle_, params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0) {
    LOG(ERROR) << "Cannot set access type: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置采样格式为16位有符号小端
  err = LATE(snd_pcm_hw_params_set_format)(handle_, params,
                                           SND_PCM_FORMAT_S16_LE);
  if (err < 0) {
    LOG(ERROR) << "Cannot set sample format: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置声道数
  err = LATE(snd_pcm_hw_params_set_channels)(handle_, params,
                                             config_.channel_count);
  if (err < 0) {
    LOG(ERROR) << "Cannot set channel count: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置采样率
  unsigned int rate = config_.sample_rate;
  err = LATE(snd_pcm_hw_params_set_rate_near)(handle_, params, &rate, 0);
  if (err < 0) {
    LOG(ERROR) << "Cannot set sample rate: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置period size (单位:帧)
  period_size_ = rate * kDefaultPeriodSizeMs / 1000;
  err = LATE(snd_pcm_hw_params_set_period_size_near)(handle_, params,
                                                     &period_size_, 0);
  if (err < 0) {
    LOG(ERROR) << "Cannot set period size: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置buffer size
  buffer_size_ = period_size_ * kDefaultBufferCount;
  err = LATE(snd_pcm_hw_params_set_buffer_size_near)(handle_, params,
                                                     &buffer_size_);
  if (err < 0) {
    LOG(ERROR) << "Cannot set buffer size: " << LATE(snd_strerror)(err);
    return err;
  }

  // 应用参数
  err = LATE(snd_pcm_hw_params)(handle_, params);
  if (err < 0) {
    LOG(ERROR) << "Cannot set hw params: " << LATE(snd_strerror)(err);
    return err;
  }

  return 0;
}

status_t AlsaAudioTrack::SetSWParams() {
  snd_pcm_sw_params_t* params;
  snd_pcm_sw_params_alloca(&params);

  // 获取当前软件参数
  int err = LATE(snd_pcm_sw_params_current)(handle_, params);
  if (err < 0) {
    LOG(ERROR) << "Cannot get sw params: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置开始阈值
  err = LATE(snd_pcm_sw_params_set_start_threshold)(handle_, params,
                                                    buffer_size_ / 2);
  if (err < 0) {
    LOG(ERROR) << "Cannot set start threshold: " << LATE(snd_strerror)(err);
    return err;
  }

  // 设置最小可用帧数
  err = LATE(snd_pcm_sw_params_set_avail_min)(handle_, params, period_size_);
  if (err < 0) {
    LOG(ERROR) << "Cannot set avail min: " << LATE(snd_strerror)(err);
    return err;
  }

  // 应用参数
  err = LATE(snd_pcm_sw_params)(handle_, params);
  if (err < 0) {
    LOG(ERROR) << "Cannot set sw params: " << LATE(snd_strerror)(err);
    return err;
  }

  return 0;
}

ssize_t AlsaAudioTrack::Write(const void* buffer, size_t size, bool blocking) {
  if (!ready_ || !playing_)
    return -EINVAL;

  const uint8_t* data = static_cast<const uint8_t*>(buffer);
  size_t frames = size / frameSize();
  ssize_t written = 0;

  while (frames > 0) {
    snd_pcm_sframes_t ret = LATE(snd_pcm_writei)(handle_, data, frames);

    if (ret == -EAGAIN) {
      if (!blocking) {
        break;
      }
      LATE(snd_pcm_wait)(handle_, -1);
      continue;
    }

    if (ret < 0) {
      if (ret == -EPIPE) {
        // 处理underrun
        LATE(snd_pcm_prepare)(handle_);
        continue;
      }
      return ret;
    }

    frames -= ret;
    data += ret * frameSize();
    written += ret;
  }

  return written * frameSize();
}

status_t AlsaAudioTrack::Start() {
  if (!ready_)
    return -EINVAL;
  if (playing_)
    return 0;

  int err = LATE(snd_pcm_prepare)(handle_);
  if (err < 0) {
    LOG(ERROR) << "Cannot prepare audio: " << LATE(snd_strerror)(err);
    return err;
  }

  err = LATE(snd_pcm_start)(handle_);
  if (err < 0) {
    LOG(ERROR) << "Cannot start audio: " << LATE(snd_strerror)(err);
    return err;
  }

  playing_ = true;
  return 0;
}

void AlsaAudioTrack::Stop() {
  if (!ready_)
    return;
  if (!playing_)
    return;

  LATE(snd_pcm_drop)(handle_);
  playing_ = false;
}

void AlsaAudioTrack::Pause() {
  if (!ready_ || !playing_)
    return;
  LATE(snd_pcm_pause)(handle_, 1);
  playing_ = false;
}

void AlsaAudioTrack::Flush() {
  if (!ready_)
    return;
  LATE(snd_pcm_drop)(handle_);
  LATE(snd_pcm_prepare)(handle_);
}

void AlsaAudioTrack::Close() {
  if (handle_) {
    Stop();
    LATE(snd_pcm_close)(handle_);
    handle_ = nullptr;
  }
  ready_ = false;
  playing_ = false;
  temp_buffer_.reset();
}

}  // namespace linux_audio
}  // namespace media
}  // namespace ave