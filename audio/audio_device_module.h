/*
 * audio_device_module.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AUDIO_DEVICE_MODULE_H
#define AUDIO_DEVICE_MODULE_H

#include <memory>

#include "base/errors.h"

namespace ave {
namespace media {

struct AudioDeviceInfo {};

class AudioTrack;
class AudioRecord;

// AudioDeviceModule is a module for creating AudioDevice objects and managing
// audio devices.
class AudioDeviceModule {
  // create an AudioTrack object.
  virtual std::shared_ptr<AudioTrack> CreateAudioTrack() = 0;

  // create an AudioRecord object.
  virtual std::shared_ptr<AudioRecord> CreateAudioRecord() = 0;

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

#endif /* !AUDIO_DEVICE_MODULE_H */
