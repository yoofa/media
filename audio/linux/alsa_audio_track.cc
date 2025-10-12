/*
 * alsa_audio_track.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/alsa_audio_track.h"

#include <algorithm>

#include "base/logging.h"
#include "base/task_util/repeating_task.h"
#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_stdlib.h"
#include "media/audio/linux/alsa_symbol_table.h"

#define LATE(sym) LATESYM_GET(AlsaSymbolTable, symbol_table_, sym)

namespace ave {
namespace media {
namespace linux_audio {

namespace {
// default ALSA buffer parameters
constexpr size_t kDefaultPeriodSizeMs = 10;  // 10ms
constexpr size_t kDefaultBufferCount = 4;    // 4 period
// constexpr int kMaxRetryCount = 3;
}  // namespace

AlsaAudioTrack::AlsaAudioTrack(AlsaSymbolTable* symbol_table)
    : symbol_table_(symbol_table),
      handle_(nullptr),
      callback_(nullptr),
      cookie_(nullptr),
      ready_(false),
      playing_(false),
      period_size_(0),
      buffer_size_(0),
      frames_written_(0),
      callback_buffer_(nullptr),
      callback_running_(false) {}

AlsaAudioTrack::~AlsaAudioTrack() {
  Close();
}

bool AlsaAudioTrack::ready() const {
  return ready_;
}

ssize_t AlsaAudioTrack::bufferSize() const {
  return static_cast<ssize_t>(buffer_size_) * frameSize();
}

ssize_t AlsaAudioTrack::frameCount() const {
  return static_cast<ssize_t>(buffer_size_);
}

ssize_t AlsaAudioTrack::channelCount() const {
  return ChannelLayoutToChannelCount(config_.channel_layout);
}

ssize_t AlsaAudioTrack::frameSize() const {
  // For PCM_16_BIT format, we use 2 bytes per sample
  return ChannelLayoutToChannelCount(config_.channel_layout) *
         2;  // 16 bits = 2 bytes
}

uint32_t AlsaAudioTrack::sampleRate() const {
  return config_.sample_rate;
}

uint32_t AlsaAudioTrack::latency() const {
  if (!ready_) {
    return 0;
  }

  snd_pcm_sframes_t delay = 0;
  if (LATE(snd_pcm_delay)(handle_, &delay) < 0) {
    return 0;
  }
  return delay * 1000 / config_.sample_rate;
}

float AlsaAudioTrack::msecsPerFrame() const {
  return 1000.0f / static_cast<float>(config_.sample_rate);
}

status_t AlsaAudioTrack::GetPosition(uint32_t* position) const {
  if (!ready_ || !position) {
    return -EINVAL;
  }

  snd_pcm_sframes_t delay{};
  snd_pcm_sframes_t avail{};

  if (LATE(snd_pcm_delay)(handle_, &delay) < 0) {
    return -EINVAL;
  }

  if (LATE(snd_pcm_avail_update)(handle_) < 0) {
    return -EINVAL;
  }

  *position = (buffer_size_ - (delay + avail)) % buffer_size_;
  return 0;
}

status_t AlsaAudioTrack::GetFramesWritten(uint32_t* frameswritten) const {
  *frameswritten = frames_written_;
  return 0;
}

int64_t AlsaAudioTrack::GetPlayedOutDurationUs(int64_t nowUs) const {
  if (!ready_) {
    return -EINVAL;
  }

  snd_pcm_sframes_t delay_frames{};
  if (LATE(snd_pcm_delay)(handle_, &delay_frames) < 0) {
    return -EINVAL;
  }

  snd_pcm_sframes_t played_frames = frames_written_ - delay_frames;
  if (played_frames < 0) {
    played_frames = 0;
  }

  long long played_us = (played_frames * 1000000LL) / config_.sample_rate;

  return played_us;
}

int64_t AlsaAudioTrack::GetBufferDurationInUs() const {
  return 0;
}

status_t AlsaAudioTrack::Open(audio_config_t config,
                              AudioCallback cb,
                              void* cookie) {
  if (ready_) {
    return -EEXIST;
  }

  config_ = config;
  callback_ = cb;
  cookie_ = cookie;

  // Open PCM device
  int err = LATE(snd_pcm_open)(&handle_, "default", SND_PCM_STREAM_PLAYBACK,
                               SND_PCM_NONBLOCK);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot open audio device: "
                      << LATE(snd_strerror)(err);
    return err;
  }

  // Set hardware parameters
  err = SetHWParams();
  if (err < 0) {
    Close();
    return err;
  }

  // Set software parameters
  err = SetSWParams();
  if (err < 0) {
    Close();
    return err;
  }

  // Initialize callback mode if callback is provided
  if (callback_) {
    size_t buffer_bytes = period_size_ * frameSize();
    callback_buffer_.reset(new uint8_t[buffer_bytes]);

    task_runner_ = std::make_unique<base::TaskRunner>(
        base::CreateTaskRunnerStdlibFactory()->CreateTaskRunner(
            "alsa_callback", base::TaskRunnerFactory::Priority::HIGH));
  }

  ready_ = true;
  return 0;
}

status_t AlsaAudioTrack::SetHWParams() {
  snd_pcm_hw_params_t* params{};
  LATE(snd_pcm_hw_params_malloc)(&params);

  // Fill params with full configuration space
  int err = LATE(snd_pcm_hw_params_any)(handle_, params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot get hw params: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  // Set access type
  err = LATE(snd_pcm_hw_params_set_access)(handle_, params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set access type: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  // Set sample format
  err = LATE(snd_pcm_hw_params_set_format)(handle_, params,
                                           SND_PCM_FORMAT_S16_LE);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set format: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  // Set channels
  err = LATE(snd_pcm_hw_params_set_channels)(
      handle_, params, ChannelLayoutToChannelCount(config_.channel_layout));
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set channels: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  // Set sample rate
  unsigned int rate = config_.sample_rate;
  err = LATE(snd_pcm_hw_params_set_rate_near)(handle_, params, &rate, nullptr);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set rate: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  // Set period size
  period_size_ = rate * kDefaultPeriodSizeMs / 1000;
  err = LATE(snd_pcm_hw_params_set_period_size_near)(handle_, params,
                                                     &period_size_, nullptr);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set period size: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  // Set buffer size
  buffer_size_ = period_size_ * kDefaultBufferCount;
  err = LATE(snd_pcm_hw_params_set_buffer_size_near)(handle_, params,
                                                     &buffer_size_);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set buffer size: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_hw_params_free)(params);
    return err;
  }

  // Apply HW params
  err = LATE(snd_pcm_hw_params)(handle_, params);
  LATE(snd_pcm_hw_params_free)(params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set hw params: " << LATE(snd_strerror)(err);
    return err;
  }

  return 0;
}

status_t AlsaAudioTrack::SetSWParams() {
  snd_pcm_sw_params_t* params{};
  LATE(snd_pcm_sw_params_malloc)(&params);

  // Get current params
  int err = LATE(snd_pcm_sw_params_current)(handle_, params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot get sw params: " << LATE(snd_strerror)(err);
    LATE(snd_pcm_sw_params_free)(params);
    return err;
  }

  // Start threshold
  err = LATE(snd_pcm_sw_params_set_start_threshold)(handle_, params,
                                                    buffer_size_ / 2);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set start threshold: "
                      << LATE(snd_strerror)(err);
    LATE(snd_pcm_sw_params_free)(params);
    return err;
  }

  // Apply SW params
  err = LATE(snd_pcm_sw_params)(handle_, params);
  LATE(snd_pcm_sw_params_free)(params);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot set sw params: " << LATE(snd_strerror)(err);
    return err;
  }

  return 0;
}

status_t AlsaAudioTrack::RecoverIfNeeded(int error) {
  if (error == -EPIPE) {  // Underrun
    error = LATE(snd_pcm_prepare)(handle_);
    if (error < 0) {
      AVE_LOG(LS_ERROR) << "Cannot recover from underrun: "
                        << LATE(snd_strerror)(error);
      return error;
    }
  } else if (error == -ESTRPIPE) {  // Suspended
    while ((error = LATE(snd_pcm_resume)(handle_)) == -EAGAIN) {
      usleep(100000);  // Wait for 100ms
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

ssize_t AlsaAudioTrack::Write(const void* buffer, size_t size, bool blocking) {
  if (!ready_ || !playing_) {
    return -EINVAL;
  }

  const auto* data = static_cast<const uint8_t*>(buffer);
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
    frames_written_ += ret;
  }

  return written * frameSize();
}

status_t AlsaAudioTrack::Start() {
  if (!ready_) {
    return -EINVAL;
  }
  if (playing_) {
    return 0;
  }

  int err = LATE(snd_pcm_prepare)(handle_);
  if (err < 0) {
    AVE_LOG(LS_ERROR) << "Cannot prepare audio: " << LATE(snd_strerror)(err);
    return err;
  }

  // Start callback thread if callback mode
  if (callback_) {
    callback_running_.store(true);
    repeating_task_ = base::RepeatingTaskHandle::Start(
        task_runner_->Get(), [this]() { return CallbackThreadFunc(); });
  } else {
    err = LATE(snd_pcm_start)(handle_);
    if (err < 0) {
      AVE_LOG(LS_ERROR) << "Cannot start audio: " << LATE(snd_strerror)(err);
      return err;
    }
  }

  playing_ = true;
  return 0;
}

void AlsaAudioTrack::Stop() {
  if (!ready_) {
    return;
  }

  if (!playing_) {
    return;
  }

  // Stop callback thread first
  if (callback_) {
    callback_running_.store(false);
    repeating_task_.Stop();
  }

  LATE(snd_pcm_drop)(handle_);
  playing_ = false;
}

void AlsaAudioTrack::Pause() {
  if (!ready_ || !playing_) {
    return;
  }
  LATE(snd_pcm_pause)(handle_, 1);
  playing_ = false;
}

void AlsaAudioTrack::Flush() {
  if (!ready_) {
    return;
  }
  LATE(snd_pcm_drop)(handle_);
  LATE(snd_pcm_prepare)(handle_);
}

void AlsaAudioTrack::Close() {
  if (handle_) {
    Stop();
    LATE(snd_pcm_close)(handle_);
    handle_ = nullptr;
  }

  callback_buffer_.reset();
  task_runner_.reset();

  ready_ = false;
  playing_ = false;
}

uint64_t AlsaAudioTrack::CallbackThreadFunc() {
  if (!callback_running_.load() || !playing_) {
    return 0;
  }

  // Check available space in the buffer
  snd_pcm_sframes_t avail = LATE(snd_pcm_avail_update)(handle_);
  if (avail < 0) {
    if (RecoverIfNeeded(avail) < 0) {
      AVE_LOG(LS_ERROR) << "Cannot recover from error: "
                        << LATE(snd_strerror)(avail);
      return 1000;  // Retry after 1ms
    }
    return 0;
  }

  // If there's enough space for at least one period, request data via callback
  if (avail >= static_cast<snd_pcm_sframes_t>(period_size_)) {
    size_t buffer_bytes = period_size_ * frameSize();

    // Request data from callback
    callback_(this, callback_buffer_.get(), buffer_bytes, cookie_,
              CB_EVENT_FILL_BUFFER);

    // Write the filled buffer to ALSA
    snd_pcm_uframes_t frames_to_write = period_size_;
    const uint8_t* data = callback_buffer_.get();

    while (frames_to_write > 0 && callback_running_.load()) {
      snd_pcm_sframes_t ret =
          LATE(snd_pcm_writei)(handle_, data, frames_to_write);

      if (ret == -EAGAIN) {
        LATE(snd_pcm_wait)(handle_, 10);
        continue;
      }

      if (ret < 0) {
        if (RecoverIfNeeded(ret) < 0) {
          AVE_LOG(LS_ERROR) << "Write error: " << LATE(snd_strerror)(ret);
          return 1000;
        }
        continue;
      }

      frames_to_write -= ret;
      data += ret * frameSize();
      frames_written_ += ret;
    }

    // Check PCM state and start if needed
    snd_pcm_state_t state = LATE(snd_pcm_state)(handle_);
    if (state == SND_PCM_STATE_PREPARED) {
      int err = LATE(snd_pcm_start)(handle_);
      if (err < 0) {
        AVE_LOG(LS_ERROR) << "Cannot start PCM: " << LATE(snd_strerror)(err);
      }
    }

    return 0;  // Continue immediately to check for more data
  }

  return 1000;  // Wait 1ms before checking again
}

}  // namespace linux_audio
}  // namespace media
}  // namespace ave
