/*
 * aaudio_audio_device.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "aaudio_audio_device.h"

#include "base/logging.h"
#include "media/audio/android/aaudio_audio_record.h"
#include "media/audio/android/aaudio_audio_track.h"

namespace ave {
namespace media {
namespace android {

AAudioAudioDevice::AAudioAudioDevice() = default;

AAudioAudioDevice::~AAudioAudioDevice() = default;

status_t AAudioAudioDevice::Init() {
  if (initialized_) {
    return OK;
  }

  // Probe AAudio availability by trying to create a stream builder
  AAudioStreamBuilder* builder = nullptr;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK) {
    AVE_LOG(LS_ERROR) << "AAudio not available: " << result;
    return INVALID_OPERATION;
  }

  // Clean up the builder since we only needed it for availability check
  AAudioStreamBuilder_delete(builder);
  initialized_ = true;
  AVE_LOG(LS_INFO) << "AAudioAudioDevice initialized successfully";
  return OK;
}

std::shared_ptr<AudioTrack> AAudioAudioDevice::CreateAudioTrack() {
  if (!initialized_) {
    AVE_LOG(LS_ERROR) << "AAudioAudioDevice not initialized";
    return nullptr;
  }
  return std::make_shared<AAudioAudioTrack>();
}

std::shared_ptr<AudioRecord> AAudioAudioDevice::CreateAudioRecord() {
  if (!initialized_) {
    AVE_LOG(LS_ERROR) << "AAudioAudioDevice not initialized";
    return nullptr;
  }
  return std::make_shared<AAudioAudioRecord>();
}

std::shared_ptr<AudioLoopback> AAudioAudioDevice::CreateAudioLoopback() {
  return nullptr;
}

std::vector<std::pair<int, AudioDeviceInfo>>
AAudioAudioDevice::GetSupportedAudioDevices() {
  return {};
}

status_t AAudioAudioDevice::SetAudioInputDevice(int device_id) {
  (void)device_id;
  return INVALID_OPERATION;
}

status_t AAudioAudioDevice::SetAudioOutputDevice(int device_id) {
  (void)device_id;
  return INVALID_OPERATION;
}

}  // namespace android
}  // namespace media
}  // namespace ave
