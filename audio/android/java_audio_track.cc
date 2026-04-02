/*
 * java_audio_track.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "java_audio_track.h"

#include "base/android/jni/jvm.h"
#include "base/logging.h"
#include "jni_headers/sdk/android/generated_avp_jni/AudioSink_jni.h"
#include "media/audio/channel_layout.h"

namespace ave {
namespace media {
namespace android {

using jni::AttachCurrentThreadIfNeeded;
using jni::Java_AudioSink_close;
using jni::Java_AudioSink_flush;
using jni::Java_AudioSink_getBufferDurationUs;
using jni::Java_AudioSink_getBufferSize;
using jni::Java_AudioSink_getChannelCount;
using jni::Java_AudioSink_getFramesWritten;
using jni::Java_AudioSink_getLatency;
using jni::Java_AudioSink_getPosition;
using jni::Java_AudioSink_getSampleRate;
using jni::Java_AudioSink_isReady;
using jni::Java_AudioSink_open;
using jni::Java_AudioSink_pause;
using jni::Java_AudioSink_start;
using jni::Java_AudioSink_stop;
using jni::Java_AudioSink_write;

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
  return static_cast<uint32_t>(
      Java_AudioSink_getLatency(env, j_audio_sink_));
}

float JavaAudioTrack::msecsPerFrame() const {
  return config_.sample_rate > 0 ? 1000.0f / config_.sample_rate : 0.0f;
}

status_t JavaAudioTrack::GetPosition(uint32_t* position) const {
  if (!j_audio_sink_.obj() || !position) {
    return -EINVAL;
  }
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  *position = static_cast<uint32_t>(
      Java_AudioSink_getPosition(env, j_audio_sink_));
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
                   << " channels=" << channel_count
                   << " encoding=" << encoding;

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
  (void)blocking;

  if (!opened_ || !j_audio_sink_.obj()) {
    return -EINVAL;
  }
  if (size == 0) {
    return 0;
  }

  JNIEnv* env = AttachCurrentThreadIfNeeded();

  // Create Java byte[] and copy native data into it
  jbyteArray j_data = env->NewByteArray(static_cast<jint>(size));
  if (!j_data) {
    AVE_LOG(LS_ERROR) << "JavaAudioTrack::Write: NewByteArray failed";
    return -ENOMEM;
  }
  env->SetByteArrayRegion(j_data, 0, static_cast<jint>(size),
                          static_cast<const jbyte*>(buffer));

  jint written = Java_AudioSink_write(
      env, j_audio_sink_,
      jni_zero::JavaParamRef<jbyteArray>(env, j_data), 0,
      static_cast<int>(size));

  env->DeleteLocalRef(j_data);

  return static_cast<ssize_t>(written);
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
  // Must match AudioSink.ENCODING_PCM_* constants
  switch (format) {
    case AUDIO_FORMAT_PCM_16_BIT:
      return 1;  // ENCODING_PCM_16BIT
    case AUDIO_FORMAT_PCM_FLOAT:
      return 2;  // ENCODING_PCM_FLOAT
    case AUDIO_FORMAT_PCM_8_BIT:
      return 3;  // ENCODING_PCM_8BIT
    case AUDIO_FORMAT_PCM_32_BIT:
      return 4;  // ENCODING_PCM_32BIT
    default:
      return 1;  // default to 16-bit
  }
}

}  // namespace android
}  // namespace media
}  // namespace ave
