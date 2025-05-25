/*
 * aaudio_audio_device.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AAUDIO_AUDIO_DEVICE_H_H_
#define AVE_MEDIA_AAUDIO_AUDIO_DEVICE_H_H_

#include <aaudio/AAudio.h>
#include <memory>
#include <vector>

#include "media/audio/audio_device.h"

namespace ave {
namespace media {
namespace android {

class AAudioAudioDevice : public AudioDevice {
 public:
  AAudioAudioDevice();
  ~AAudioAudioDevice() override;

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
  bool initialized_ = false;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_AAUDIO_AUDIO_DEVICE_H_H_ */
