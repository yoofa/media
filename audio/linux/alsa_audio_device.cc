/*
 * alsa_audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "alsa_audio_device.h"

#include <alsa/asoundlib.h>
#include <algorithm>

#include "base/logging.h"
#include "media/audio/alsa/alsa_audio_track.h"
#include "media/audio/linux/latebinding_symbol_table_linux.h"

namespace ave {
namespace media {

std::unique_ptr<AlsaAudioDevice> AlsaAudioDevice::Create() {
  std::unique_ptr<AlsaSymbolTable> alsa_symbols(new AlsaSymbolTable());
  if (!alsa_symbols->Load()) {
    AVE_LOG(LS_ERROR) << "Failed to load ALSA symbols";
    return nullptr;
  }
  
  return std::unique_ptr<AlsaAudioDevice>(
      new AlsaAudioDevice(std::move(alsa_symbols)));
}

AlsaAudioDevice::AlsaAudioDevice(std::unique_ptr<AlsaSymbolTable> alsa_symbols)
    : alsa_symbols_(std::move(alsa_symbols)),
      current_input_device_id_(-1),
      current_output_device_id_(-1) {
  EnumerateDevices();
}

AlsaAudioDevice::~AlsaAudioDevice() = default;

std::shared_ptr<AudioTrack> AlsaAudioDevice::CreateAudioTrack() {
  return std::make_shared<AlsaAudioTrack>(alsa_symbols_.get());
}

std::shared_ptr<AudioRecord> AlsaAudioDevice::CreateAudioRecord() {
  // TODO: Implement AlsaAudioRecord
  return nullptr;
}

std::shared_ptr<AudioLoopback> AlsaAudioDevice::CreateAudioLoopback() {
  // TODO: Implement AlsaAudioLoopback
  return nullptr;
}

std::vector<std::pair<int, AudioDeviceInfo>> 
AlsaAudioDevice::GetSupportedAudioDevices() {
  return audio_devices_;
}

status_t AlsaAudioDevice::SetAudioInputDevice(int device_id) {
  auto it = std::find_if(
      audio_devices_.begin(), audio_devices_.end(),
      [device_id](const auto& pair) { return pair.first == device_id; });

  if (it == audio_devices_.end()) {
    return -EINVAL;
  }

  current_input_device_id_ = device_id;
  return 0;
}

status_t AlsaAudioDevice::SetAudioOutputDevice(int device_id) {
  auto it = std::find_if(
      audio_devices_.begin(), audio_devices_.end(),
      [device_id](const auto& pair) { return pair.first == device_id; });

  if (it == audio_devices_.end()) {
    return -EINVAL;
  }

  current_output_device_id_ = device_id;
  return 0;
}

void AlsaAudioDevice::EnumerateDevices() {
  int card = -1;
  snd_ctl_t* handle;
  snd_ctl_card_info_t* info;
  
  // Use symbol table to call ALSA functions
  alsa_symbols_->snd_ctl_card_info_alloca(&info);

  while (alsa_symbols_->snd_card_next(&card) >= 0 && card >= 0) {
    char name[32];
    snprintf(name, sizeof(name), "hw:%d", card);

    if (alsa_symbols_->snd_ctl_open(&handle, name, 0) < 0) {
      continue;
    }

    if (alsa_symbols_->snd_ctl_card_info(handle, info) < 0) {
      alsa_symbols_->snd_ctl_close(handle);
      continue;
    }

    // Add device info to audio_devices_
    AudioDeviceInfo device_info;
    // TODO: Fill device_info with ALSA card information using symbol table
    audio_devices_.emplace_back(card, device_info);

    alsa_symbols_->snd_ctl_close(handle);
  }
}

}  // namespace media
}  // namespace ave



