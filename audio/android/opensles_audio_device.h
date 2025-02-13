/*
 * opensles_audio_device.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_AUDIO_ANDROID_OPENSLES_AUDIO_DEVICE_H_
#define MEDIA_AUDIO_ANDROID_OPENSLES_AUDIO_DEVICE_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <memory>

#include "media/audio/audio_device.h"

namespace ave {
namespace media {
namespace android {

class OpenSLESAudioDevice : public AudioDevice {
 public:
  OpenSLESAudioDevice();
  ~OpenSLESAudioDevice() override;

  // AudioDevice implementation
  status_t Init() override;
  std::shared_ptr<AudioTrack> CreateAudioTrack() override;
  std::shared_ptr<AudioRecord> CreateAudioRecord() override;
  std::shared_ptr<AudioLoopback> CreateAudioLoopback() override;

  std::vector<std::pair<int, AudioDeviceInfo>> GetSupportedAudioDevices()
      override;
  status_t SetAudioInputDevice(int device_id) override;
  status_t SetAudioOutputDevice(int device_id) override;

 private:
  // OpenSL ES engine interfaces
  SLObjectItf engine_object_ = nullptr;
  SLEngineItf engine_engine_ = nullptr;
  SLObjectItf output_mix_object_ = nullptr;

  bool initialized_ = false;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif /* !MEDIA_AUDIO_ANDROID_OPENSLES_AUDIO_DEVICE_H_ */
