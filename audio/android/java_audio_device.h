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
 * AudioDevice implementation backed by a Java AudioDevice (factory).
 *
 * CreateAudioTrack() calls Java AudioDevice.createAudioSink() to obtain a
 * Java AudioSink, then wraps it in a JavaAudioTrack for native use.
 */
class JavaAudioDevice : public AudioDevice {
 public:
  // |j_audio_device| is a global reference to a Java AudioDevice object.
  explicit JavaAudioDevice(
      const jni_zero::ScopedJavaGlobalRef<jobject>& j_audio_device);
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
  bool IsFormatSupported(audio_format_t format) override;

 private:
  // Convert audio_format_t to AudioSink.ENCODING_* constant for JNI call.
  static int ToAudioSinkEncoding(audio_format_t format);

  jni_zero::ScopedJavaGlobalRef<jobject> j_audio_device_;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_JAVA_AUDIO_DEVICE_H
