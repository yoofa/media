/*
 * opensles_audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "opensles_audio_device.h"

#include "base/logging.h"
#include "media/audio/android/opensles_audio_track.h"

namespace ave {
namespace media {
namespace android {

OpenSLESAudioDevice::OpenSLESAudioDevice() = default;

OpenSLESAudioDevice::~OpenSLESAudioDevice() {
  if (output_mix_object_) {
    (*output_mix_object_)->Destroy(output_mix_object_);
  }
  if (engine_object_) {
    (*engine_object_)->Destroy(engine_object_);
  }
}

status_t OpenSLESAudioDevice::Init() {
  if (initialized_) {
    return OK;
  }

  // Create engine
  SLresult result =
      slCreateEngine(&engine_object_, 0, nullptr, 0, nullptr, nullptr);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to create OpenSL ES engine: " << result;
    return INVALID_OPERATION;
  }

  // Realize the engine
  result = (*engine_object_)->Realize(engine_object_, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to realize OpenSL ES engine: " << result;
    return INVALID_OPERATION;
  }

  // Get the engine interface
  result = (*engine_object_)
               ->GetInterface(engine_object_, SL_IID_ENGINE, &engine_engine_);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to get OpenSL ES engine interface: " << result;
    return INVALID_OPERATION;
  }

  // Create output mix
  result = (*engine_engine_)
               ->CreateOutputMix(engine_engine_, &output_mix_object_, 0,
                                 nullptr, nullptr);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to create OpenSL ES output mix: " << result;
    return INVALID_OPERATION;
  }

  // Realize the output mix
  result = (*output_mix_object_)->Realize(output_mix_object_, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to realize OpenSL ES output mix: " << result;
    return INVALID_OPERATION;
  }

  initialized_ = true;
  return OK;
}

std::shared_ptr<AudioTrack> OpenSLESAudioDevice::CreateAudioTrack() {
  if (!initialized_) {
    AVE_LOG(LS_ERROR) << "OpenSLESAudioDevice not initialized";
    return nullptr;
  }

  return std::make_shared<OpenSLESAudioTrack>(engine_engine_,
                                              output_mix_object_);
}

std::shared_ptr<AudioRecord> OpenSLESAudioDevice::CreateAudioRecord() {
  return nullptr;
}

std::shared_ptr<AudioLoopback> OpenSLESAudioDevice::CreateAudioLoopback() {
  return nullptr;
}

std::vector<std::pair<int, AudioDeviceInfo>>
OpenSLESAudioDevice::GetSupportedAudioDevices() {
  return {};
}

status_t OpenSLESAudioDevice::SetAudioInputDevice(int device_id) {
  return INVALID_OPERATION;
}

status_t OpenSLESAudioDevice::SetAudioOutputDevice(int device_id) {
  return INVALID_OPERATION;
}

}  // namespace android
}  // namespace media
}  // namespace ave
