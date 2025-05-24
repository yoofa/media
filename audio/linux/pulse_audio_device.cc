/*
 * pulse_audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/pulse_audio_device.h"

#include "base/logging.h"
#include "media/audio/linux/pulse_audio_record.h"
#include "media/audio/linux/pulse_audio_track.h"
#include "media/audio/linux/pulse_symbol_table.h"

namespace ave {
namespace media {
namespace linux_audio {

status_t PulseAudioDevice::Init() {
  if (!GetPulseSymbolTable()->Load()) {
    AVE_LOG(LS_ERROR) << "PulseAudio library not available";
    return -ENODEV;
  }
  return OK;
}

std::shared_ptr<AudioTrack> PulseAudioDevice::CreateAudioTrack() {
  return std::make_shared<PulseAudioTrack>();
}

std::shared_ptr<AudioRecord> PulseAudioDevice::CreateAudioRecord() {
  return std::make_shared<PulseAudioRecord>();
}

std::shared_ptr<AudioLoopback> PulseAudioDevice::CreateAudioLoopback() {
  return nullptr;
}

std::vector<std::pair<int, AudioDeviceInfo>>
PulseAudioDevice::GetSupportedAudioDevices() {
  // Minimal: return a default sink/source with common config
  std::vector<std::pair<int, AudioDeviceInfo>> devices;
  AudioDeviceInfo out{};
  out.direction = AudioDeviceDirection::kOutput;
  out.name = "pulse-default-output";
  AudioConfig cfg{};
  cfg.sample_rate = 48000;
  cfg.channel_layout = CHANNEL_LAYOUT_STEREO;
  cfg.format = AUDIO_FORMAT_PCM_16_BIT;
  out.supported_configs.push_back(cfg);
  devices.emplace_back(0, out);

  AudioDeviceInfo in{};
  in.direction = AudioDeviceDirection::kInput;
  in.name = "pulse-default-input";
  in.supported_configs.push_back(cfg);
  devices.emplace_back(1, in);
  return devices;
}

status_t PulseAudioDevice::SetAudioInputDevice(int device_id) {
  (void)device_id;  // For now, using default source
  return OK;
}

status_t PulseAudioDevice::SetAudioOutputDevice(int device_id) {
  (void)device_id;  // For now, using default sink
  return OK;
}

}  // namespace linux_audio
}  // namespace media
}  // namespace ave
