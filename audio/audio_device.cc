/*
 * audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "audio_device.h"

namespace ave {
namespace media {

std::shared_ptr<AudioDevice> AudioDevice::CreateAudioDevice(
    AudioDevice::PlatformType type) {
  std::shared_ptr<AudioDevice> audio_device;

  switch (type) {
    case kDefault: {
      break;
    }
    case kLinuxAlsa: {
      break;
    }
    case kLinuxPulse: {
      break;
    }
    case kAndroidJava: {
      break;
    }
    case kAndroidOpenSLES: {
      break;
    }
    case kAndroidAAudio: {
      break;
    }
    default:
    case kDummy: {
      break;
    }
  }
  return audio_device;
}

}  // namespace media

}  // namespace ave
