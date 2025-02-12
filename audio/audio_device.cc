/*
 * audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "audio_device.h"
#include "linux/alsa_audio_device.h"

namespace ave {
namespace media {

std::shared_ptr<AudioDevice> AudioDevice::CreateAudioDevice(
    AudioDevice::PlatformType type) {
  std::shared_ptr<AudioDevice> audio_device;

  switch (type) {
    case PlatformType::kDefault: {
      break;
    }
    case PlatformType::kLinuxAlsa: {
      audio_device = std::make_shared<linux_audio::AlsaAudioDevice>();
      break;
    }
    case PlatformType::kLinuxPulse: {
      break;
    }
    case PlatformType::kAndroidJava: {
      break;
    }
    case PlatformType::kAndroidOpenSLES: {
      break;
    }
    case PlatformType::kAndroidAAudio: {
      break;
    }
    default:
    case PlatformType::kDummy: {
      break;
    }
  }
  return audio_device;
}

}  // namespace media

}  // namespace ave
