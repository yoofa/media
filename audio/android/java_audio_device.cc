/*
 * java_audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "java_audio_device.h"

#include "base/android/jni/jvm.h"
#include "base/logging.h"
#include "jni_headers/media/android/generated_media_jni/AudioDevice_jni.h"
#include "media/audio/android/java_audio_track.h"

namespace ave {
namespace media {
namespace android {

using ave::jni::AttachCurrentThreadIfNeeded;
using media::jni::Java_AudioDevice_createAudioSink;

JavaAudioDevice::JavaAudioDevice(
    const jni_zero::ScopedJavaGlobalRef<jobject>& j_audio_device)
    : j_audio_device_(j_audio_device) {}

JavaAudioDevice::~JavaAudioDevice() = default;

status_t JavaAudioDevice::Init() {
  if (!j_audio_device_.obj()) {
    AVE_LOG(LS_ERROR) << "JavaAudioDevice::Init: no AudioDevice";
    return INVALID_OPERATION;
  }
  AVE_LOG(LS_INFO) << "JavaAudioDevice::Init: OK";
  return 0;
}

std::shared_ptr<AudioTrack> JavaAudioDevice::CreateAudioTrack() {
  AVE_LOG(LS_INFO) << "JavaAudioDevice::CreateAudioTrack";
  if (!j_audio_device_.obj()) {
    AVE_LOG(LS_ERROR) << "JavaAudioDevice::CreateAudioTrack: no AudioDevice";
    return nullptr;
  }

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Call Java AudioDevice.createAudioSink() → returns an AudioSink jobject.
  auto j_audio_sink = Java_AudioDevice_createAudioSink(env, j_audio_device_);
  if (!j_audio_sink.obj()) {
    AVE_LOG(LS_ERROR)
        << "JavaAudioDevice::CreateAudioTrack: createAudioSink returned null";
    return nullptr;
  }

  // Wrap the Java AudioSink in a C++ JavaAudioTrack.
  jni_zero::ScopedJavaGlobalRef<jobject> global_sink(env, j_audio_sink.obj());
  return std::make_shared<JavaAudioTrack>(global_sink);
}

std::shared_ptr<AudioRecord> JavaAudioDevice::CreateAudioRecord() {
  // Playback only; recording not supported via Java path.
  return nullptr;
}

std::shared_ptr<AudioLoopback> JavaAudioDevice::CreateAudioLoopback() {
  return nullptr;
}

std::vector<std::pair<int, AudioDeviceInfo>>
JavaAudioDevice::GetSupportedAudioDevices() {
  return {};
}

status_t JavaAudioDevice::SetAudioInputDevice(int device_id) {
  (void)device_id;
  return INVALID_OPERATION;
}

status_t JavaAudioDevice::SetAudioOutputDevice(int device_id) {
  (void)device_id;
  return INVALID_OPERATION;
}

}  // namespace android
}  // namespace media
}  // namespace ave
