/*
 * aaudio_audio_track.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "aaudio_audio_track.h"

#include <chrono>
#include <cstring>

#include "base/attributes.h"
#include "base/logging.h"
#include "media/audio/channel_layout.h"

namespace ave {
namespace media {
namespace android {

namespace {

constexpr int32_t kRequestedBufferDurationMs = 10;
constexpr int32_t kTargetBufferBursts = 2;
constexpr int64_t kRouteTeardownDelayMs = 100;
constexpr uint32_t kMaxAutomaticRestartAttempts = 1;

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

AAudioAudioTrack::AAudioAudioTrack() = default;

AAudioAudioTrack::~AAudioAudioTrack() {
  Close();
}

bool AAudioAudioTrack::ready() const {
  return stream_ != nullptr;
}

ssize_t AAudioAudioTrack::bufferSize() const {
  if (!stream_) {
    return 0;
  }
  int32_t frames = AAudioStream_getBufferSizeInFrames(stream_);
  return frames > 0 ? frames * frameSize() : 0;
}

ssize_t AAudioAudioTrack::frameCount() const {
  if (!stream_) {
    return 0;
  }
  int32_t frames = AAudioStream_getBufferSizeInFrames(stream_);
  return frames > 0 ? frames : 0;
}

ssize_t AAudioAudioTrack::channelCount() const {
  return ChannelLayoutToChannelCount(config_.channel_layout);
}

ssize_t AAudioAudioTrack::frameSize() const {
  // 16-bit PCM assumed for now if unspecified
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

uint32_t AAudioAudioTrack::sampleRate() const {
  return config_.sample_rate;
}

uint32_t AAudioAudioTrack::latency() const {
  if (!stream_ || config_.sample_rate == 0) {
    return 0;
  }
  // Use actual buffer size for latency, not just burst size
  int32_t buffer_frames = AAudioStream_getBufferSizeInFrames(stream_);
  if (buffer_frames <= 0) {
    return 0;
  }
  return static_cast<uint32_t>((static_cast<int64_t>(buffer_frames) * 1000) /
                               config_.sample_rate);
}

float AAudioAudioTrack::msecsPerFrame() const {
  return 1000.0f / static_cast<float>(config_.sample_rate);
}

status_t AAudioAudioTrack::GetTimestamp(AudioTimestamp* ts) const {
  if (!stream_ || !ts) {
    return -EINVAL;
  }
  int64_t hardware_frames = 0;
  int64_t hardware_time_ns = 0;
  aaudio_result_t result = AAudioStream_getTimestamp(
      stream_, CLOCK_MONOTONIC, &hardware_frames, &hardware_time_ns);
  if (result != AAUDIO_OK) {
    return -EINVAL;
  }
  ts->position = static_cast<uint32_t>(hardware_frames);
  ts->nanos = hardware_time_ns;
  return OK;
}

int64_t AAudioAudioTrack::GetBufferDurationInUs() const {
  if (!stream_ || config_.sample_rate == 0) {
    return 0;
  }

  // Use AAudioStream_getTimestamp for accurate remaining-buffer latency.
  // This returns the frame count that has been presented at the hardware DAC
  // and the system time at which that presentation occurred.
  int64_t hardware_frames = 0;
  int64_t hardware_time_ns = 0;
  aaudio_result_t result = AAudioStream_getTimestamp(
      stream_, CLOCK_MONOTONIC, &hardware_frames, &hardware_time_ns);
  if (result == AAUDIO_OK) {
    int64_t frames_written = AAudioStream_getFramesWritten(stream_);
    int64_t buffered = frames_written - hardware_frames;
    if (buffered > 0) {
      return buffered * 1000000LL / static_cast<int64_t>(config_.sample_rate);
    }
    return 0;
  }

  // Fallback: use current buffer size as an approximation
  int32_t buffer_frames = AAudioStream_getBufferSizeInFrames(stream_);
  if (buffer_frames > 0) {
    return static_cast<int64_t>(buffer_frames) * 1000000LL /
           static_cast<int64_t>(config_.sample_rate);
  }
  return 0;
}

status_t AAudioAudioTrack::Open(audio_config_t config,
                                AudioCallback cb,
                                void* cookie) {
  std::lock_guard<std::mutex> lock(stream_mutex_);
  if (stream_) {
    return -EEXIST;
  }

  config_ = config;
  callback_ = cb;
  cookie_ = cookie;
  shutting_down_ = false;
  restart_attempt_count_ = 0;
  return OpenLocked();
}

status_t AAudioAudioTrack::OpenLocked() {
  const int32_t requested_capacity = static_cast<int32_t>(
      static_cast<int64_t>(config_.sample_rate) * kRequestedBufferDurationMs /
      1000);
  const auto open_stream = [this, requested_capacity](
                               aaudio_sharing_mode_t sharing_mode) {
    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t open_result = AAudio_createStreamBuilder(&builder);
    if (open_result != AAUDIO_OK || !builder) {
      AVE_LOG(LS_ERROR) << "AAudio_createStreamBuilder failed: "
                        << open_result;
      return open_result;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(
        builder, static_cast<int32_t>(config_.sample_rate));
    AAudioStreamBuilder_setChannelCount(
        builder, ChannelLayoutToChannelCount(config_.channel_layout));
    AAudioStreamBuilder_setFormat(builder, ToAAudioFormat(config_.format));
    AAudioStreamBuilder_setPerformanceMode(
        builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, sharing_mode);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, requested_capacity);
    if (__builtin_available(android 28, *)) {
      AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_MEDIA);
      AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH);
    }

    if (callback_) {
      AAudioStreamBuilder_setDataCallback(builder, DataCallback, this);
      AAudioStreamBuilder_setErrorCallback(builder, ErrorCallback, this);
    }

    open_result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);
    return open_result;
  };

  aaudio_result_t result = open_stream(AAUDIO_SHARING_MODE_EXCLUSIVE);
  if (result != AAUDIO_OK || !stream_) {
    AVE_LOG(LS_INFO) << "Exclusive AAudio output unavailable (" << result
                     << "), retrying shared output";
    stream_ = nullptr;
    result = open_stream(AAUDIO_SHARING_MODE_SHARED);
  }
  if (result != AAUDIO_OK || !stream_) {
    AVE_LOG(LS_ERROR) << "Failed to open AAudio stream: " << result;
    stream_ = nullptr;
    return INVALID_OPERATION;
  }

  const int32_t frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
  if (frames_per_burst > 0) {
    (void)AAudioStream_setBufferSizeInFrames(
        stream_, frames_per_burst * kTargetBufferBursts);
  }

  AVE_LOG(LS_INFO) << "AAudio output: sharing="
                    << AAudioStream_getSharingMode(stream_)
                    << ", performance="
                    << AAudioStream_getPerformanceMode(stream_)
                    << ", sample_rate=" << AAudioStream_getSampleRate(stream_)
                    << ", frames_per_burst=" << frames_per_burst
                    << ", buffer_capacity="
                    << AAudioStream_getBufferCapacityInFrames(stream_)
                    << ", buffer_size="
                    << AAudioStream_getBufferSizeInFrames(stream_);

  return 0;
}

status_t AAudioAudioTrack::Start() {
  std::lock_guard<std::mutex> lock(stream_mutex_);
  return StartLocked();
}

status_t AAudioAudioTrack::StartLocked() {
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

void AAudioAudioTrack::Stop() {
  started_ = false;
  std::lock_guard<std::mutex> lock(stream_mutex_);
  if (!stream_) {
    return;
  }
  (void)AAudioStream_requestStop(stream_);
}

void AAudioAudioTrack::Flush() {
  // AAudio has no explicit flush for output, stop/start handles it
}

void AAudioAudioTrack::Pause() {
  started_ = false;
  std::lock_guard<std::mutex> lock(stream_mutex_);
  if (!stream_) {
    return;
  }
  (void)AAudioStream_requestPause(stream_);
}

void AAudioAudioTrack::Close() {
  shutting_down_ = true;
  started_ = false;
  {
    std::lock_guard<std::mutex> restart_lock(restart_mutex_);
    if (restart_thread_.joinable()) {
      restart_thread_.request_stop();
    }
  }
  if (restart_thread_.joinable()) {
    restart_thread_.join();
  }

  std::lock_guard<std::mutex> stream_lock(stream_mutex_);
  if (stream_) {
    (void)AAudioStream_requestStop(stream_);
    AAudioStream_close(stream_);
    stream_ = nullptr;
  }
}

void AAudioAudioTrack::ScheduleRestartAfterDisconnect() {
  if (shutting_down_ || !started_ || restart_pending_.exchange(true)) {
    return;
  }

  const uint32_t previous_attempt_count = restart_attempt_count_.fetch_add(1);
  if (previous_attempt_count >= kMaxAutomaticRestartAttempts) {
    restart_pending_ = false;
    AVE_LOG(LS_ERROR) << "AAudio output disconnected again; automatic restart "
                      << "is disabled until the next session";
    return;
  }

  AVE_LOG(LS_INFO) << "AAudio output disconnected; scheduling restart";

  std::lock_guard<std::mutex> restart_lock(restart_mutex_);
  if (shutting_down_ || !started_) {
    restart_pending_ = false;
    return;
  }
  if (restart_thread_.joinable()) {
    restart_thread_.join();
  }
  restart_thread_ = std::jthread(
      [this](std::stop_token stop_token) { RestartAfterDisconnect(stop_token); });
}

void AAudioAudioTrack::RestartAfterDisconnect(std::stop_token stop_token) {
  // AAudio invokes ErrorCallback on an internal audio thread. Let that callback
  // return before closing the invalid stream and opening a new one.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if (stop_token.stop_requested() || shutting_down_ || !started_) {
    restart_pending_ = false;
    return;
  }

  {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    if (stop_token.stop_requested() || shutting_down_ || !started_) {
      restart_pending_ = false;
      return;
    }
    if (stream_) {
      AAudioStream_close(stream_);
      stream_ = nullptr;
    }
  }

  // AudioPolicy tears down a direct output asynchronously. Reopening before
  // its release completes can make policy select the primary output instead.
  std::this_thread::sleep_for(
      std::chrono::milliseconds(kRouteTeardownDelayMs));
  AVE_LOG(LS_INFO) << "Reopening AAudio output after device-route change";
  std::lock_guard<std::mutex> stream_lock(stream_mutex_);
  if (stop_token.stop_requested() || shutting_down_ || !started_) {
    restart_pending_ = false;
    return;
  }
  const status_t open_status = OpenLocked();
  if (open_status != OK) {
    AVE_LOG(LS_ERROR) << "Failed to reopen disconnected AAudio output: "
                      << open_status;
    restart_pending_ = false;
    return;
  }
  const status_t start_status = StartLocked();
  if (start_status != OK) {
    AVE_LOG(LS_ERROR) << "Failed to restart disconnected AAudio output: "
                      << start_status;
  } else {
    AVE_LOG(LS_INFO) << "Restarted AAudio output after device-route change";
  }
  restart_pending_ = false;
}

ssize_t AAudioAudioTrack::Write(const void* buffer,
                                size_t size,
                                bool blocking) {
  if (!stream_ || callback_) {
    return -EINVAL;  // callback mode doesn't use Write
  }
  auto frames = static_cast<int32_t>(size / frameSize());
  if (frames <= 0) {
    return 0;
  }

  // Single-shot write: use a short timeout in blocking mode so the caller's
  // mutex is not held for a long time.  The caller (RenderFrameInternal)
  // handles partial writes by retaining cached_frame_ and retrying.
  // A 2 ms timeout lets AAudio drain one burst before giving up, which is
  // enough to make steady progress without stalling other threads.
  int64_t timeout_ns = blocking ? 2 * 1000000LL : 0;
  int32_t written = AAudioStream_write(stream_, buffer, frames, timeout_ns);

  if (written < 0) {
    return static_cast<ssize_t>(written);  // propagate error code
  }
  return static_cast<ssize_t>(written * frameSize());
}

aaudio_data_callback_result_t AAudioAudioTrack::DataCallback(
    AAudioStream* stream AVE_MAYBE_UNUSED,
    void* userData,
    void* audioData,
    int32_t numFrames) {
  auto* self = static_cast<AAudioAudioTrack*>(userData);
  if (!self || !self->callback_) {
    return AAUDIO_CALLBACK_RESULT_STOP;
  }
  size_t bytes = static_cast<size_t>(numFrames) * self->frameSize();
  self->callback_(self, audioData, bytes, self->cookie_, CB_EVENT_FILL_BUFFER);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioAudioTrack::ErrorCallback(AAudioStream* stream AVE_MAYBE_UNUSED,
                                     void* userData,
                                     aaudio_result_t error) {
  AVE_LOG(LS_ERROR) << "AAudio stream error: " << error;
  if (error == AAUDIO_ERROR_DISCONNECTED && userData != nullptr) {
    static_cast<AAudioAudioTrack*>(userData)->ScheduleRestartAfterDisconnect();
  }
}

}  // namespace android
}  // namespace media
}  // namespace ave
