/*
 * pulse_audio_record.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/pulse_audio_record.h"

#include "base/logging.h"
#include "media/audio/channel_layout.h"
#include "media/audio/linux/pulse_symbol_table.h"

#define LATE(sym)                                             \
  LATESYM_GET(ave::media::linux_audio::PulseAudioSymbolTable, \
              GetPulseSymbolTable(), sym)

namespace ave {
namespace media {
namespace linux_audio {

namespace {
constexpr size_t kDefaultLatencyMs = 20;  // 20ms
}

PulseAudioRecord::PulseAudioRecord() = default;
PulseAudioRecord::~PulseAudioRecord() {
  Close();
}

bool PulseAudioRecord::ready() const {
  return ready_;
}
ssize_t PulseAudioRecord::bufferSize() const {
  return static_cast<ssize_t>(buffer_size_);
}
ssize_t PulseAudioRecord::frameCount() const {
  return static_cast<ssize_t>(buffer_size_ / frameSize());
}
ssize_t PulseAudioRecord::channelCount() const {
  return ChannelLayoutToChannelCount(config_.channel_layout);
}
ssize_t PulseAudioRecord::frameSize() const {
  return ChannelLayoutToChannelCount(config_.channel_layout) * 2;  // 16-bit
}
uint32_t PulseAudioRecord::sampleRate() const {
  return config_.sample_rate;
}
uint32_t PulseAudioRecord::latency() const {
  return 0;
}

status_t PulseAudioRecord::GetPosition(uint32_t* position) const {
  if (!position) {
    return -EINVAL;
  }
  *position = static_cast<uint32_t>(frames_read_);
  return 0;
}
int64_t PulseAudioRecord::GetRecordedDurationUs(
    int64_t nowUs AVE_MAYBE_UNUSED) const {
  // TODO(youfa)
  return 0;
}
status_t PulseAudioRecord::GetFramesRead(uint32_t* frames_read) const {
  if (!frames_read) {
    return -EINVAL;
  }
  *frames_read = static_cast<uint32_t>(frames_read_);
  return 0;
}

void PulseAudioRecord::ContextStateCallback(pa_context* c, void* userdata) {
  auto* self = static_cast<PulseAudioRecord*>(userdata);
  switch (LATE(pa_context_get_state)(c)) {
    case PA_CONTEXT_READY:
    case PA_CONTEXT_TERMINATED:
    case PA_CONTEXT_FAILED:
      LATE(pa_threaded_mainloop_signal)(self->mainloop_, 0);
      break;
    default:
      break;
  }
}

void PulseAudioRecord::StreamStateCallback(pa_stream* s AVE_MAYBE_UNUSED,
                                           void* userdata AVE_MAYBE_UNUSED) {
  auto* self = static_cast<PulseAudioRecord*>(userdata);
  LATE(pa_threaded_mainloop_signal)(self->mainloop_, 0);
}

void PulseAudioRecord::StreamReadCallback(pa_stream* s AVE_MAYBE_UNUSED,
                                          size_t length AVE_MAYBE_UNUSED,
                                          void* userdata) {
  auto* self = static_cast<PulseAudioRecord*>(userdata);
  if (!self->callback_) {
    // If no callback, drop data
    const void* data = nullptr;
    size_t bytes = 0;
    if (LATE(pa_stream_peek)(s, &data, &bytes) == 0 && bytes > 0) {
      LATE(pa_stream_drop)(s);
    }
    return;
  }

  const void* data = nullptr;
  size_t bytes = 0;
  if (LATE(pa_stream_peek)(s, &data, &bytes) < 0 || bytes == 0 ||
      data == nullptr) {
    return;
  }

  // Invoke callback with captured data
  self->callback_(self, const_cast<void*>(data), bytes, self->cookie_,
                  CB_EVENT_MORE_DATA);
  self->frames_read_ += bytes / self->frameSize();
  LATE(pa_stream_drop)(s);
}

void PulseAudioRecord::StreamOverflowCallback(pa_stream* s AVE_MAYBE_UNUSED,
                                              void* userdata AVE_MAYBE_UNUSED) {
  AVE_LOG(LS_WARNING) << "PulseAudio stream overflow";
}

status_t PulseAudioRecord::InitPulseAudio() {
  if (!GetPulseSymbolTable()->Load()) {
    AVE_LOG(LS_ERROR) << "Failed to load PulseAudio symbols";
    return -ENODEV;
  }

  mainloop_ = LATE(pa_threaded_mainloop_new)();
  if (!mainloop_) {
    return -ENOMEM;
  }
  pa_mainloop_api* api = LATE(pa_threaded_mainloop_get_api)(mainloop_);
  context_ = LATE(pa_context_new)(api, "AVE Audio Record");
  if (!context_) {
    return -ENOMEM;
  }

  int err = LATE(pa_threaded_mainloop_start)(mainloop_);
  if (err < 0) {
    return err;
  }

  LATE(pa_threaded_mainloop_lock)(mainloop_);
  LATE(pa_context_set_state_callback)(context_, ContextStateCallback, this);
  err =
      LATE(pa_context_connect)(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
  if (err < 0) {
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return err;
  }

  for (;;) {
    pa_context_state_t st = LATE(pa_context_get_state)(context_);
    if (st == PA_CONTEXT_READY) {
      break;
    }
    if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED) {
      LATE(pa_threaded_mainloop_unlock)(mainloop_);
      return -EIO;
    }
    LATE(pa_threaded_mainloop_wait)(mainloop_);
  }
  LATE(pa_threaded_mainloop_unlock)(mainloop_);
  return 0;
}

status_t PulseAudioRecord::Open(audio_config_t config,
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

  pa_sample_spec spec = {
      .format = PA_SAMPLE_S16LE,
      .rate = static_cast<uint32_t>(config_.sample_rate),
      .channels = static_cast<uint8_t>(
          ChannelLayoutToChannelCount(config_.channel_layout))};

  LATE(pa_threaded_mainloop_lock)(mainloop_);
  stream_ = LATE(pa_stream_new)(context_, "Record", &spec, nullptr);
  if (!stream_) {
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return -ENOMEM;
  }

  LATE(pa_stream_set_state_callback)(stream_, StreamStateCallback, this);
  LATE(pa_stream_set_read_callback)(stream_, StreamReadCallback, this);
  LATE(pa_stream_set_overflow_callback)(stream_, StreamOverflowCallback, this);

  pa_buffer_attr attr{};
  attr.fragsize = static_cast<uint32_t>(spec.rate * spec.channels * 2 *
                                        kDefaultLatencyMs / 1000);
  attr.maxlength = static_cast<uint32_t>(-1);

  int err = LATE(pa_stream_connect_record)(
      stream_, nullptr, &attr,
      static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY |
                                     PA_STREAM_AUTO_TIMING_UPDATE));
  if (err < 0) {
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return err;
  }

  while (LATE(pa_stream_get_state)(stream_) != PA_STREAM_READY) {
    LATE(pa_threaded_mainloop_wait)(mainloop_);
  }
  LATE(pa_threaded_mainloop_unlock)(mainloop_);

  buffer_size_ =
      static_cast<size_t>(config_.sample_rate) *
      static_cast<size_t>(ChannelLayoutToChannelCount(config_.channel_layout)) *
      2 * kDefaultLatencyMs / 1000;
  ready_ = true;
  return 0;
}

ssize_t PulseAudioRecord::Read(void* buffer, size_t size, bool blocking) {
  if (!ready_ || !recording_) {
    return -EINVAL;
  }
  LATE(pa_threaded_mainloop_lock)(mainloop_);
  size_t readable = LATE(pa_stream_readable_size)(stream_);
  while (readable == 0 && blocking) {
    LATE(pa_threaded_mainloop_wait)(mainloop_);
    readable = LATE(pa_stream_readable_size)(stream_);
  }
  size_t to_read = std::min(size, readable);
  const void* data = nullptr;
  size_t bytes = 0;
  if (to_read == 0) {
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return 0;
  }
  int err = LATE(pa_stream_peek)(stream_, &data, &bytes);
  if (err < 0 || bytes == 0 || data == nullptr) {
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
    return 0;
  }
  size_t n = std::min(bytes, to_read);
  memcpy(buffer, data, n);
  frames_read_ += n / frameSize();
  LATE(pa_stream_drop)(stream_);
  LATE(pa_threaded_mainloop_unlock)(mainloop_);
  return static_cast<ssize_t>(n);
}

status_t PulseAudioRecord::Start() {
  if (!ready_) {
    return -EINVAL;
  }
  recording_ = true;
  return 0;
}

void PulseAudioRecord::Stop() {
  if (!ready_) {
    return;
  }
  recording_ = false;
}

void PulseAudioRecord::Flush() {
  // No explicit flush needed; drop pending
}

void PulseAudioRecord::Pause() {
  Stop();
}

void PulseAudioRecord::Close() {
  if (stream_) {
    LATE(pa_threaded_mainloop_lock)(mainloop_);
    LATE(pa_stream_disconnect)(stream_);
    LATE(pa_stream_unref)(stream_);
    stream_ = nullptr;
    LATE(pa_threaded_mainloop_unlock)(mainloop_);
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
  recording_ = false;
}

status_t PulseAudioRecord::SetGain(float gain) {
  gain_ = gain;
  return 0;
}
float PulseAudioRecord::GetGain() const {
  return gain_;
}

}  // namespace linux_audio
}  // namespace media
}  // namespace ave
