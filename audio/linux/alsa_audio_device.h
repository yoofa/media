/*
 * alsa_audio_device.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_ALSA_AUDIO_DEVICE_H_H_
#define AVE_MEDIA_ALSA_AUDIO_DEVICE_H_H_

#include <memory>
#include <vector>

#include "media/audio/audio_device.h"
#include "media/audio/linux/alsa_symbol_table.h"

namespace ave {
namespace media {
namespace linux_audio {

class AlsaAudioDevice : public AudioDevice {
 public:
  // Creates an AlsaAudioDevice. Returns nullptr if ALSA shared library 
  // cannot be loaded or required symbols are missing.
  AlsaAudioDevice();
  ~AlsaAudioDevice() override;

  // AudioDevice implementation
  status_t Init() override;
  std::shared_ptr<AudioTrack> CreateAudioTrack() override;
  std::shared_ptr<AudioRecord> CreateAudioRecord() override;
  std::shared_ptr<AudioLoopback> CreateAudioLoopback() override;
  std::vector<std::pair<int, AudioDeviceInfo>> GetSupportedAudioDevices() override;
  status_t SetAudioInputDevice(int device_id) override;
  status_t SetAudioOutputDevice(int device_id) override;

 private:
  // Helper function to enumerate ALSA devices
  void EnumerateDevices();

  bool _initialized;
  std::unique_ptr<AlsaSymbolTable> alsa_symbols_;
  std::vector<std::pair<int, AudioDeviceInfo>> audio_devices_;
  int current_input_device_id_;
  int current_output_device_id_;
};

}  // namespace linux_audio
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_ALSA_AUDIO_DEVICE_H_H_
