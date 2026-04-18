/*
 * java_audio_track.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "java_audio_track.h"

#include <chrono>
#include <thread>

#include "base/android/jni/jvm.h"
#include "base/logging.h"
#include "base/time_utils.h"
#include "jni_headers/media/android/generated_media_jni/AudioSink_jni.h"
#include "media/audio/audio_format.h"
#include "media/audio/channel_layout.h"

namespace ave {
namespace media {
namespace android {

using ave::jni::AttachCurrentThreadIfNeeded;
using media::jni::Java_AudioSink_close;
using media::jni::Java_AudioSink_flush;
using media::jni::Java_AudioSink_getBufferDurationUs;
using media::jni::Java_AudioSink_getBufferSize;
using media::jni::Java_AudioSink_getChannelCount;
using media::jni::Java_AudioSink_getFramesWritten;
using media::jni::Java_AudioSink_getLatency;
using media::jni::Java_AudioSink_getPosition;
using media::jni::Java_AudioSink_getSampleRate;
using media::jni::Java_AudioSink_isReady;
using media::jni::Java_AudioSink_open;
using media::jni::Java_AudioSink_pause;
using media::jni::Java_AudioSink_start;
using media::jni::Java_AudioSink_stop;
using media::jni::Java_AudioSink_write;

JavaAudioTrack::JavaAudioTrack(
    const jni_zero::ScopedJavaGlobalRef<jobject>& j_audio_sink)
    : j_audio_sink_(j_audio_sink) {}

JavaAudioTrack::~JavaAudioTrack() {
  Close();
}

bool JavaAudioTrack::ready() const {
  if (!j_audio_sink_.obj()) {
    return false;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return Java_AudioSink_isReady(env, j_audio_sink_);
}

ssize_t JavaAudioTrack::bufferSize() const {
  if (!j_audio_sink_.obj()) {
    return 0;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return Java_AudioSink_getBufferSize(env, j_audio_sink_);
}

ssize_t JavaAudioTrack::frameCount() const {
  ssize_t fs = frameSize();
  return fs > 0 ? bufferSize() / fs : 0;
}

ssize_t JavaAudioTrack::channelCount() const {
  if (!j_audio_sink_.obj()) {
    return ChannelLayoutToChannelCount(config_.channel_layout);
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return Java_AudioSink_getChannelCount(env, j_audio_sink_);
}

ssize_t JavaAudioTrack::frameSize() const {
  // Compressed formats: frame size is 1 (each buffer is one access unit).
  uint32_t main_format = config_.format & AUDIO_FORMAT_MAIN_MASK;
  if (main_format != AUDIO_FORMAT_PCM && main_format != AUDIO_FORMAT_DEFAULT) {
    return 1;
  }

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

uint32_t JavaAudioTrack::sampleRate() const {
  if (!j_audio_sink_.obj()) {
    return config_.sample_rate;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return static_cast<uint32_t>(
      Java_AudioSink_getSampleRate(env, j_audio_sink_));
}

uint32_t JavaAudioTrack::latency() const {
  if (!j_audio_sink_.obj()) {
    return 0;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return static_cast<uint32_t>(Java_AudioSink_getLatency(env, j_audio_sink_));
}

float JavaAudioTrack::msecsPerFrame() const {
  return config_.sample_rate > 0 ? 1000.0f / config_.sample_rate : 0.0f;
}

status_t JavaAudioTrack::GetPosition(uint32_t* position) const {
  if (!j_audio_sink_.obj() || !position) {
    return -EINVAL;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  *position =
      static_cast<uint32_t>(Java_AudioSink_getPosition(env, j_audio_sink_));
  return 0;
}

int64_t JavaAudioTrack::GetPlayedOutDurationUs(int64_t nowUs) const {
  (void)nowUs;
  return GetBufferDurationInUs();
}

status_t JavaAudioTrack::GetFramesWritten(uint32_t* frameswritten) const {
  if (!j_audio_sink_.obj() || !frameswritten) {
    return -EINVAL;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  *frameswritten = static_cast<uint32_t>(
      Java_AudioSink_getFramesWritten(env, j_audio_sink_));
  return 0;
}

int64_t JavaAudioTrack::GetBufferDurationInUs() const {
  if (!j_audio_sink_.obj()) {
    return 0;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return Java_AudioSink_getBufferDurationUs(env, j_audio_sink_);
}

status_t JavaAudioTrack::Open(audio_config_t config,
                              AudioCallback cb,
                              void* cookie) {
  (void)cb;
  (void)cookie;

  if (opened_) {
    return -EEXIST;
  }
  if (!j_audio_sink_.obj()) {
    AVE_LOG(LS_ERROR) << "JavaAudioTrack::Open: no AudioSink";
    return INVALID_OPERATION;
  }

  config_ = config;

  int channel_count = ChannelLayoutToChannelCount(config.channel_layout);
  int encoding = ToAudioSinkEncoding(config.format);

  AVE_LOG(LS_INFO) << "JavaAudioTrack::Open: sampleRate=" << config.sample_rate
                   << " channels=" << channel_count << " encoding=" << encoding;

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  bool ok = Java_AudioSink_open(env, j_audio_sink_,
                                static_cast<int>(config.sample_rate),
                                channel_count, encoding);
  if (!ok) {
    AVE_LOG(LS_ERROR) << "JavaAudioTrack::Open: Java AudioSink.open failed";
    return INVALID_OPERATION;
  }

  opened_ = true;
  return 0;
}

ssize_t JavaAudioTrack::Write(const void* buffer, size_t size, bool blocking) {
  return Write(buffer, size, blocking, 0);
}

ssize_t JavaAudioTrack::Write(const void* buffer,
                              size_t size,
                              bool blocking,
                              uint32_t frame_count) {
  if (!opened_ || !j_audio_sink_.obj()) {
    return -EINVAL;
  }
  if (size == 0) {
    return 0;
  }

  JNIEnv* env = AttachCurrentThreadIfNeeded();

  jobject j_buffer = env->NewDirectByteBuffer(const_cast<void*>(buffer),
                                              static_cast<jlong>(size));
  if (!j_buffer) {
    AVE_LOG(LS_ERROR) << "JavaAudioTrack::Write: NewDirectByteBuffer failed";
    return -ENOMEM;
  }

  size_t total_written = 0;
  const bool compressed =
      (config_.format & AUDIO_FORMAT_MAIN_MASK) != AUDIO_FORMAT_PCM &&
      (config_.format & AUDIO_FORMAT_MAIN_MASK) != AUDIO_FORMAT_DEFAULT;
  // Compressed/offload writes are retried via cached_frame_ and a short
  // renderer reschedule. Do not spin here for long periods when the sink
  // applies back-pressure, or startup position polling and video sync stall.
  const int64_t blocking_deadline_ms =
      blocking ? base::TimeMillis() + (compressed ? 20 : 200) : 0;
  while (total_written < size) {
    jint written = Java_AudioSink_write(
        env, j_audio_sink_, jni_zero::JavaParamRef<jobject>(env, j_buffer),
        static_cast<int>(size - total_written), static_cast<int>(frame_count));
    if (written == 0 && blocking && base::TimeMillis() < blocking_deadline_ms) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
    }
    if (written <= 0) {
      env->DeleteLocalRef(j_buffer);
      if (total_written > 0 && !blocking) {
        return static_cast<ssize_t>(total_written);
      }
      if (total_written > 0 && blocking) {
        AVE_LOG(LS_WARNING)
            << "JavaAudioTrack::Write: blocking write incomplete, requested="
            << size << " written=" << total_written
            << " last_result=" << written;
      }
      return total_written > 0 ? static_cast<ssize_t>(total_written)
                               : static_cast<ssize_t>(written);
    }

    total_written += static_cast<size_t>(written);
    if (total_written < size) {
      env->DeleteLocalRef(j_buffer);
      j_buffer = env->NewDirectByteBuffer(
          const_cast<uint8_t*>(static_cast<const uint8_t*>(buffer) +
                               total_written),
          static_cast<jlong>(size - total_written));
      if (!j_buffer) {
        AVE_LOG(LS_ERROR)
            << "JavaAudioTrack::Write: NewDirectByteBuffer retry failed";
        return static_cast<ssize_t>(total_written);
      }
    }
    if (!blocking) {
      break;
    }
  }

  env->DeleteLocalRef(j_buffer);

  return static_cast<ssize_t>(total_written);
}

status_t JavaAudioTrack::Start() {
  if (!opened_ || !j_audio_sink_.obj()) {
    return -EINVAL;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioSink_start(env, j_audio_sink_);
  return 0;
}

void JavaAudioTrack::Stop() {
  if (!opened_ || !j_audio_sink_.obj()) {
    return;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioSink_stop(env, j_audio_sink_);
}

void JavaAudioTrack::Flush() {
  if (!opened_ || !j_audio_sink_.obj()) {
    return;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioSink_flush(env, j_audio_sink_);
}

void JavaAudioTrack::Pause() {
  if (!opened_ || !j_audio_sink_.obj()) {
    return;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioSink_pause(env, j_audio_sink_);
}

void JavaAudioTrack::Close() {
  if (!opened_ || !j_audio_sink_.obj()) {
    return;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_AudioSink_close(env, j_audio_sink_);
  opened_ = false;
}

int JavaAudioTrack::ToAudioSinkEncoding(audio_format_t format) {
  // Must match AudioSink.ENCODING_* constants in AudioSink.java
  switch (format) {
    case AUDIO_FORMAT_PCM_16_BIT:
      return 1;  // ENCODING_PCM_16BIT
    case AUDIO_FORMAT_PCM_FLOAT:
      return 2;  // ENCODING_PCM_FLOAT
    case AUDIO_FORMAT_PCM_8_BIT:
      return 3;  // ENCODING_PCM_8BIT
    case AUDIO_FORMAT_PCM_32_BIT:
      return 4;  // ENCODING_PCM_32BIT
    case AUDIO_FORMAT_AC3:
      return 10;  // ENCODING_AC3
    case AUDIO_FORMAT_E_AC3:
    case AUDIO_FORMAT_E_AC3_JOC:
      return 11;  // ENCODING_E_AC3
    case AUDIO_FORMAT_DTS:
      return 12;  // ENCODING_DTS
    case AUDIO_FORMAT_DTS_HD:
      return 13;  // ENCODING_DTS_HD
    case AUDIO_FORMAT_AAC_LC:
      return 14;  // ENCODING_AAC_LC
    case AUDIO_FORMAT_DOLBY_TRUEHD:
      return 15;  // ENCODING_DOLBY_TRUEHD
    case AUDIO_FORMAT_AC4:
      return 17;  // ENCODING_AC4
    default:
      // For any AAC variant, use AAC_LC passthrough
      if ((format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_AAC ||
          (format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_AAC_ADTS ||
          (format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_AAC_LATM) {
        return 14;  // ENCODING_AAC_LC
      }
      return 1;  // default to 16-bit PCM
  }
}

}  // namespace android
}  // namespace media
}  // namespace ave
