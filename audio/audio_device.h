/*
 * audio_device.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include <memory>
#include <vector>

#include "base/errors.h"

#include "media/audio/audio_loopback.h"
#include "media/audio/audio_record.h"
#include "media/audio/audio_track.h"

namespace ave {
namespace media {

enum class AudioDeviceDirection : uint8_t {
  kInput = 0,
  kOutput,
  kLoopback,
  kNone,
};

struct AudioDeviceInfo {
  AudioDeviceDirection direction = AudioDeviceDirection::kNone;
  std::string_view name;
  std::vector<AudioConfig> supported_configs;
};

// AudioDevice is a module for creating AudioDevice objects and managing
// audio devices.
class AudioDevice {
 public:
  enum class PlatformType : uint8_t {
    kDefault = 0,
    kLinuxAlsa,
    kLinuxPulse,
    kAndroidJava,
    kAndroidOpenSLES,
    kAndroidAAudio,
    kDummy,
  };

  static std::shared_ptr<AudioDevice> CreateAudioDevice(
      PlatformType type = PlatformType::kDefault);

  /* implement the following functions in the platform-specific code */
  virtual ~AudioDevice() = default;

  // initialize the audio device
  virtual status_t Init() = 0;

  // create an AudioTrack object.
  virtual std::shared_ptr<AudioTrack> CreateAudioTrack() = 0;

  // create an AudioRecord object.
  virtual std::shared_ptr<AudioRecord> CreateAudioRecord() = 0;

  // create an AudioLoopback object.
  virtual std::shared_ptr<AudioLoopback> CreateAudioLoopback() = 0;

  // get supported audio devices
  // return <device_id, device_info>
  virtual std::vector<std::pair<int, AudioDeviceInfo>>
  GetSupportedAudioDevices() = 0;

  // set audio input device
  virtual status_t SetAudioInputDevice(int device_id) = 0;

  // set audio output device
  virtual status_t SetAudioOutputDevice(int device_id) = 0;
};

}  // namespace media
}  // namespace ave

#endif /* !AUDIO_DEVICE_H */
