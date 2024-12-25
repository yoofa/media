/*
 * alsa_audio_device_module.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef ALSA_AUDIO_DEVICE_MODULE_H
#define ALSA_AUDIO_DEVICE_MODULE_H

#include "media/audio/audio_device_module.h"

namespace ave {
namespace media {

class AlsaAudioDeviceModule : public AudioDeviceModule {
 public:
  AlsaAudioDeviceModule() = default;
  ~AlsaAudioDeviceModule() = default;
  std::shared_ptr<AudioTrack> CreateAudioTrack() override;
  std::shared_ptr<AudioRecord> CreateAudioRecord() override;
  std::vector<std::pair<int, AudioDeviceInfo>> GetSupportedAudioDevices()
      override;
  status_t SetAudioInputDevice(int device_id) override;
  status_t SetAudioOutputDevice(int device_id) override;
};

}  // namespace media
}  // namespace ave

#endif /* !ALSA_AUDIO_DEVICE_MODULE_H */
