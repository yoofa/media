/*
 * audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "audio_device.h"

#if defined(AVE_LINUX)
#include "linux/alsa_audio_device.h"
#include "linux/pulse_audio_device.h"
#endif

#if defined(AVE_ANDROID)
#include "android/aaudio_audio_device.h"
#include "android/opensles_audio_device.h"
#endif

namespace ave {
namespace media {

namespace {
AudioDevice::PlatformType DetectPlatform() {
  // TODO: implement platform detection
#if defined(AVE_ANDROID)
  return AudioDevice::PlatformType::kAndroidAAudio;
#elif defined(AVE_LINUX)
  return AudioDevice::PlatformType::kLinuxAlsa;
#endif
}

}  // namespace

std::shared_ptr<AudioDevice> AudioDevice::CreateAudioDevice(
    AudioDevice::PlatformType type) {
  if (type == PlatformType::kDefault) {
    type = DetectPlatform();
  }
  std::shared_ptr<AudioDevice> audio_device;

  switch (type) {
#if !defined(AVE_ANDROID) && defined(AVE_LINUX)
    case PlatformType::kLinuxAlsa: {
      audio_device = std::make_shared<linux_audio::AlsaAudioDevice>();
      break;
    }
    case PlatformType::kLinuxPulse: {
      audio_device = std::make_shared<linux_audio::PulseAudioDevice>();
      break;
    }
#endif

#if defined(AVE_ANDROID)
    case PlatformType::kAndroidJava: {
      break;
    }
    case PlatformType::kAndroidOpenSLES: {
      audio_device = std::make_shared<android::OpenSLESAudioDevice>();
      break;
    }
    case PlatformType::kAndroidAAudio: {
      audio_device = std::make_shared<android::AAudioAudioDevice>();
      break;
    }
#endif

    case PlatformType::kDefault: {
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
