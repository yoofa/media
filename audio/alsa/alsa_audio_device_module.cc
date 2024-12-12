/*
 * alsa_audio_device_module.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "alsa_audio_device_module.h"

#include "base/attributes.h"

#include "media/audio/alsa/alsa_audio_record.h"
#include "media/audio/alsa/alsa_audio_track.h"
#include "media/audio/audio_record.h"
#include "media/audio/audio_track.h"

namespace ave {
namespace media {

std::shared_ptr<AudioTrack> AlsaAudioDeviceModule::CreateAudioTrack() {
  return std::make_shared<AlsaAudioTrack>();
}

std::shared_ptr<AudioRecord> AlsaAudioDeviceModule::CreateAudioRecord() {
  return std::make_shared<AlsaAudioRecord>();
}

std::vector<std::pair<int, AudioDeviceInfo>>
AlsaAudioDeviceModule::GetSupportedAudioDevices() {
  return {};
}

// TODO: implement, remove unused
status_t AlsaAudioDeviceModule::SetAudioInputDevice(
    int device_id AVE_MAYBE_UNUSED) {
  return OK;
}

// TODO: implement, remove unused
status_t AlsaAudioDeviceModule::SetAudioOutputDevice(
    int device_id AVE_MAYBE_UNUSED) {
  // TODO
  return OK;
}

}  // namespace media
}  // namespace ave
