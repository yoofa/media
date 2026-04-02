/*
 * java_audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "java_audio_device.h"

#include "base/logging.h"
#include "media/audio/android/java_audio_track.h"

namespace ave {
namespace media {
namespace android {

JavaAudioDevice::JavaAudioDevice(
    const jni_zero::ScopedJavaGlobalRef<jobject>& j_audio_sink)
    : j_audio_sink_(j_audio_sink) {}

JavaAudioDevice::~JavaAudioDevice() = default;

status_t JavaAudioDevice::Init() {
  if (!j_audio_sink_.obj()) {
    AVE_LOG(LS_ERROR) << "JavaAudioDevice::Init: no AudioSink";
    return INVALID_OPERATION;
  }
  AVE_LOG(LS_INFO) << "JavaAudioDevice::Init: OK";
  return 0;
}

std::shared_ptr<AudioTrack> JavaAudioDevice::CreateAudioTrack() {
  AVE_LOG(LS_INFO) << "JavaAudioDevice::CreateAudioTrack";
  return std::make_shared<JavaAudioTrack>(j_audio_sink_);
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
