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
#include "media/audio/linux/alsa_audio_track.h"
#include "media/audio/linux/alsa_symbol_table.h"
#include "media/audio/linux/latebinding_symbol_table.h"

// Accesses ALSA functions through our late-binding symbol table instead of
// directly. This way we don't have to link to libasound, which means our binary
// will work on systems that don't have it.
#define LATE(sym) \
  LATESYM_GET(ave::media::linux::AlsaSymbolTable, GetAlsaSymbolTable(), sym)

// Redefine these here to be able to do late-binding
#undef snd_ctl_card_info_alloca
#define snd_ctl_card_info_alloca(ptr)                  \
  do {                                                 \
    *ptr = (snd_ctl_card_info_t*)__builtin_alloca(     \
        LATE(snd_ctl_card_info_sizeof)());             \
    memset(*ptr, 0, LATE(snd_ctl_card_info_sizeof)()); \
  } while (0)

#undef snd_pcm_info_alloca
#define snd_pcm_info_alloca(pInfo)                                           \
  do {                                                                       \
    *pInfo = (snd_pcm_info_t*)__builtin_alloca(LATE(snd_pcm_info_sizeof)()); \
    memset(*pInfo, 0, LATE(snd_pcm_info_sizeof)());                          \
  } while (0)

namespace ave {
namespace media {
namespace linux_audio {

static const unsigned int ALSA_PLAYOUT_FREQ = 48000;
static const unsigned int ALSA_PLAYOUT_CH = 2;
static const unsigned int ALSA_PLAYOUT_LATENCY = 40 * 1000;  // in us
static const unsigned int ALSA_CAPTURE_FREQ = 48000;
static const unsigned int ALSA_CAPTURE_CH = 2;
static const unsigned int ALSA_CAPTURE_LATENCY = 40 * 1000;  // in us
static const unsigned int ALSA_CAPTURE_WAIT_TIMEOUT = 5;     // in ms

AlsaAudioDevice::AlsaAudioDevice()
    : _initialized(false),
      current_input_device_id_(-1),
      current_output_device_id_(-1) {
  EnumerateDevices();
}

AlsaAudioDevice::~AlsaAudioDevice() = default;

status_t AlsaAudioDevice::Init() {
  // Load libasound
  if (!GetAlsaSymbolTable()->Load()) {
    // Alsa is not installed on this system
    AVE_LOG(LS_ERROR) << "failed to load symbol table";
    return -EINVAL;
  }
  if (_initialized) {
    return OK;
  }
  _initialized = true;
  return OK;
}

std::shared_ptr<AudioTrack> AlsaAudioDevice::CreateAudioTrack() {
  return std::make_shared<AlsaAudioTrack>(GetAlsaSymbolTable());
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
  // int card = -1;
  // snd_ctl_t* handle;
  // snd_ctl_card_info_t* info;
}

}  // namespace linux_audio
}  // namespace media
}  // namespace ave



