/*
 * java_audio_device.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_JAVA_AUDIO_DEVICE_H
#define AVE_MEDIA_JAVA_AUDIO_DEVICE_H

#include <jni.h>

#include "media/audio/audio_device.h"
#include "third_party/jni_zero/java_refs.h"

namespace ave {
namespace media {
namespace android {

/**
 * AudioDevice implementation that creates JavaAudioTrack instances.
 * Each JavaAudioTrack delegates to a Java AudioSink for actual audio output.
 */
class JavaAudioDevice : public AudioDevice {
 public:
  // |j_audio_sink| is a global reference to a Java AudioSink object.
  explicit JavaAudioDevice(
      const jni_zero::ScopedJavaGlobalRef<jobject>& j_audio_sink);
  ~JavaAudioDevice() override;

  // AudioDevice interface
  status_t Init() override;
  std::shared_ptr<AudioTrack> CreateAudioTrack() override;
  std::shared_ptr<AudioRecord> CreateAudioRecord() override;
  std::shared_ptr<AudioLoopback> CreateAudioLoopback() override;
  std::vector<std::pair<int, AudioDeviceInfo>> GetSupportedAudioDevices()
      override;
  status_t SetAudioInputDevice(int device_id) override;
  status_t SetAudioOutputDevice(int device_id) override;

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> j_audio_sink_;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_JAVA_AUDIO_DEVICE_H
