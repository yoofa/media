/*
 * pulse_audio_track.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/pulse_audio_track.h"

#include <algorithm>

#include "base/logging.h"
#include "media/audio/linux/pulse_symbol_table.h"

#define LATE(sym)                                        \
  LATESYM_GET(ave::media::linux_audio::PulseSymbolTable, \
              GetPulseSymbolTable(), sym)

namespace ave {
namespace media {
namespace linux_audio {

namespace {
// 默认的buffer参数
constexpr size_t kDefaultLatencyMs = 20;  // 20ms latency
}  // namespace

PulseAudioTrack::PulseAudioTrack()
    : mainloop_(nullptr),
      context_(nullptr),
      stream_(nullptr),
      callback_(nullptr),
      cookie_(nullptr),
      ready_(false),
      playing_(false),
      buffer_size_(0),
      temp_buffer_(nullptr),
      temp_buffer_size_(0) {}

PulseAudioTrack::~PulseAudioTrack() {
  Close();
}

void PulseAudioTrack::StreamStateCallback(pa_stream* s, void* userdata) {
  PulseAudioTrack* track = static_cast<PulseAudioTrack*>(userdata);
  pa_stream_state_t state = LATE(pa_stream_get_state)(s);

  switch (state) {
    case PA_STREAM_READY:
      LOG(INFO) << "Stream is ready";
      break;
    case PA_STREAM_FAILED:
      LOG(ERROR) << "Stream has failed";
      break;
    case PA_STREAM_TERMINATED:
      LOG(INFO) << "Stream was terminated";
      break;
    default:
      break;
  }

  LATE(pa_threaded_mainloop_signal)(track->mainloop_, 0);
}

void PulseAudioTrack::StreamWriteCallback(pa_stream* s,
                                          size_t length,
                                          void* userdata) {
  PulseAudioTrack* track = static_cast<PulseAudioTrack*>(userdata);
  if (track->callback_) {
    track->callback_(track, track->temp_buffer_.get(), length, track->cookie_,
                     CB_EVENT_FILL_BUFFER);
  }
}

void PulseAudioTrack::StreamUnderflowCallback(pa_stream* s, void* userdata) {
  LOG(WARNING) << "Stream underflow occurred";
}

status_t PulseAudioTrack::InitPulseAudio() {
  // 加载PulseAudio符号表
  if (!GetPulseSymbolTable()->Load()) {
    LOG(ERROR) << "Failed to load PulseAudio symbols";
    return -ENODEV;
  }

  // 创建主循环
  mainloop_ = LATE(pa_threaded_mainloop_new)();
  if (!mainloop_) {
    LOG(ERROR) << "Failed to create mainloop";
    return -ENOMEM;
  }

  // 获取mainloop API
  pa_mainloop_api* mainloop_api = LATE(pa_threaded_mainloop_get_api)(mainloop_);

  // 创建context
  context_ = LATE(pa_context_new)(mainloop_api, "AVE Audio");
  if (!context_) {
    LOG(ERROR) << "Failed to create context";
    return -ENOMEM;
  }

  // 启动主循环
  int err = LATE(pa_threaded_mainloop_start)(mainloop_);
  if (err < 0) {
    LOG(ERROR) << "Failed to start mainloop";
    return err;
  }

  // 连接到服务器
  LATE(pa_threaded_mainloop_lock)(mainloop_);
  err =
      LATE(pa_context_connect)(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
  if (err < 0) {
    LOG(ERROR) << "Failed to connect to PulseAudio server";
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return err;
  }

  // 等待连接完成
  while (LATE(pa_context_get_state)(context_) != PA_CONTEXT_READY) {
    LATE(pa_threaded_mainloop_wait)(mainloop_);
  }

  LATE(pa_threaded_mainloop_unlock)(mainloop_);
  return 0;
}

status_t PulseAudioTrack::Open(audio_config_t config,
                               AudioCallback cb,
                               void* cookie) {
  if (ready_) {
    return -EEXIST;
  }

  config_ = config;
  callback_ = cb;
  cookie_ = cookie;

  status_t res = InitPulseAudio();
  if (res != 0) {
    Close();
    return res;
  }

  // 创建采样规格
  pa_sample_spec spec = {
      .format = PA_SAMPLE_S16LE,
      .rate = config_.sample_rate,
      .channels = static_cast<uint8_t>(config_.channel_count)};

  // 创建stream
  LATE(pa_threaded_mainloop_lock)(mainloop_);
  stream_ = LATE(pa_stream_new)(context_, "Playback", &spec, nullptr);
  if (!stream_) {
    LOG(ERROR) << "Failed to create stream";
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return -ENOMEM;
  }

  // 设置回调
  LATE(pa_stream_set_state_callback)(stream_, StreamStateCallback, this);
  LATE(pa_stream_set_write_callback)(stream_, StreamWriteCallback, this);
  LATE(pa_stream_set_underflow_callback)
  (stream_, StreamUnderflowCallback, this);

  // 设置buffer属性
  pa_buffer_attr attr = {
      .maxlength = static_cast<uint32_t>(-1),
      .tlength = config_.sample_rate * config_.channel_count * 2 *
                 kDefaultLatencyMs / 1000,
      .prebuf = static_cast<uint32_t>(-1),
      .minreq = static_cast<uint32_t>(-1),
  };

  // 连接到sink
  int err = LATE(pa_stream_connect_playback)(
      stream_, nullptr, &attr,
      static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY |
                                     PA_STREAM_AUTO_TIMING_UPDATE),
      nullptr, nullptr);

  if (err < 0) {
    LOG(ERROR) << "Failed to connect playback stream";
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return err;
  }

  // 等待stream就绪
  while (LATE(pa_stream_get_state)(stream_) != PA_STREAM_READY) {
    LATE(pa_threaded_mainloop_wait)(mainloop_);
  }

  LATE(pa_threaded_mainloop_unlock)(mainloop_);

  // 分配临时buffer
  buffer_size_ = config_.sample_rate * config_.channel_count * 2 *
                 kDefaultLatencyMs / 1000;
  temp_buffer_ = std::make_unique<uint8_t[]>(buffer_size_);

  ready_ = true;
  return 0;
}

ssize_t PulseAudioTrack::Write(const void* buffer, size_t size, bool blocking) {
  if (!ready_ || !playing_)
    return -EINVAL;

  LATE(pa_threaded_mainloop_lock)(mainloop_);

  size_t writable = LATE(pa_stream_writable_size)(stream_);
  if (writable == 0) {
    if (!blocking) {
      LATE(pa_threaded_mainloop_unlock)(mainloop_);
      return 0;
    }
    while (writable == 0) {
      LATE(pa_threaded_mainloop_wait)(mainloop_);
      writable = LATE(pa_stream_writable_size)(stream_);
    }
  }

  size_t write_size = std::min(size, writable);
  int err = LATE(pa_stream_write)(stream_, buffer, write_size, nullptr, 0,
                                  PA_SEEK_RELATIVE);

  LATE(pa_threaded_mainloop_unlock)(mainloop_);

  if (err < 0) {
    LOG(ERROR) << "Failed to write to stream: " << LATE(pa_strerror)(err);
    return err;
  }

  return write_size;
}

status_t PulseAudioTrack::Start() {
  if (!ready_)
    return -EINVAL;
  if (playing_)
    return 0;

  playing_ = true;
  return 0;
}

void PulseAudioTrack::Stop() {
  if (!ready_)
    return;
  if (!playing_)
    return;

  LATE(pa_threaded_mainloop_lock)(mainloop_);
  LATE(pa_stream_disconnect)(stream_);
  LATE(pa_threaded_mainloop_unlock)(mainloop_);

  playing_ = false;
}

void PulseAudioTrack::Pause() {
  Stop();
}

void PulseAudioTrack::Flush() {
  if (!ready_)
    return;

  Stop();
  Start();
}

void PulseAudioTrack::Close() {
  if (stream_) {
    Stop();
    LATE(pa_stream_unref)(stream_);
    stream_ = nullptr;
  }

  if (context_) {
    LATE(pa_context_disconnect)(context_);
    LATE(pa_context_unref)(context_);
    context_ = nullptr;
  }

  if (mainloop_) {
    LATE(pa_threaded_mainloop_stop)(mainloop_);
    LATE(pa_threaded_mainloop_free)(mainloop_);
    mainloop_ = nullptr;
  }

  ready_ = false;
  playing_ = false;
  temp_buffer_.reset();
}

bool PulseAudioTrack::ready() const {
  return ready_;
}

ssize_t PulseAudioTrack::bufferSize() const {
  return buffer_size_;
}

ssize_t PulseAudioTrack::frameCount() const {
  return buffer_size_ / frameSize();
}

ssize_t PulseAudioTrack::channelCount() const {
  return config_.channel_count;
}

ssize_t PulseAudioTrack::frameSize() const {
  return config_.channel_count * 2;  // 16-bit samples
}

uint32_t PulseAudioTrack::sampleRate() const {
  return config_.sample_rate;
}

uint32_t PulseAudioTrack::latency() const {
  if (!ready_ || !playing_)
    return 0;

  LATE(pa_threaded_mainloop_lock)(mainloop_);

  pa_usec_t latency;
  int negative;

  if (LATE(pa_stream_get_latency)(stream_, &latency, &negative) < 0) {
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return 0;
  }

  LATE(pa_threaded_mainloop_unlock)(mainloop_);

  return latency * 1000 / config_.sample_rate;
}

float PulseAudioTrack::msecsPerFrame() const {
  return 1000.0f / config_.sample_rate;
}

status_t PulseAudioTrack::GetPosition(uint32_t* position) const {
  if (!ready_ || !position)
    return -EINVAL;

  // PulseAudio doesn't provide direct position information
  // We could calculate it based on written frames and latency
  *position = 0;
  return 0;
}

int64_t PulseAudioTrack::GetPlayedOutDurationUs(int64_t nowUs) const {
  if (!ready_)
    return 0;

  // TODO: Implement based on written frames and latency
  return 0;
}

status_t PulseAudioTrack::GetFramesWritten(uint32_t* frameswritten) const {
  if (!ready_ || !frameswritten)
    return -EINVAL;

  // TODO: Track written frames
  *frameswritten = 0;
  return 0;
}

int64_t PulseAudioTrack::GetBufferDurationInUs() const {
  return (buffer_size_ / frameSize()) * 1000000LL / config_.sample_rate;
}

}  // namespace linux_audio
}  // namespace media
}  // namespace ave