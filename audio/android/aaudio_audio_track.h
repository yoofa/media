/*
 * aaudio_audio_track.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AAUDIO_AUDIO_TRACK_H_H_
#define AVE_MEDIA_AAUDIO_AUDIO_TRACK_H_H_

#include <aaudio/AAudio.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "media/audio/audio_track.h"

namespace ave {
namespace media {
namespace android {

class AAudioAudioTrack : public AudioTrack {
 public:
  AAudioAudioTrack();
  ~AAudioAudioTrack() override;

  // AudioTrack implementation
  bool ready() const override;
  ssize_t bufferSize() const override;
  ssize_t frameCount() const override;
  ssize_t channelCount() const override;
  ssize_t frameSize() const override;
  uint32_t sampleRate() const override;
  uint32_t latency() const override;
  float msecsPerFrame() const override;

  status_t GetTimestamp(AudioTimestamp* ts) const override;
  int64_t GetBufferDurationInUs() const override;

  status_t Open(audio_config_t config, AudioCallback cb, void* cookie) override;
  status_t Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Close() override;
  ssize_t Write(const void* buffer, size_t size, bool blocking) override;

 private:
  status_t OpenLocked();
  status_t StartLocked();
  void ScheduleRestartAfterDisconnect();
  void RestartAfterDisconnect(std::stop_token stop_token);

  static aaudio_data_callback_result_t DataCallback(AAudioStream* stream,
                                                    void* userData,
                                                    void* audioData,
                                                    int32_t numFrames);
  static void ErrorCallback(AAudioStream* stream,
                            void* userData,
                            aaudio_result_t error);

  AudioConfig config_{};
  AudioCallback callback_;
  void* cookie_ = nullptr;

  std::mutex stream_mutex_;
  std::mutex restart_mutex_;
  std::jthread restart_thread_;
  AAudioStream* stream_ = nullptr;
  std::atomic_bool started_ = false;
  std::atomic_bool shutting_down_ = false;
  std::atomic_bool restart_pending_ = false;
  std::atomic_uint32_t restart_attempt_count_ = 0;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_AAUDIO_AUDIO_TRACK_H_H_ */
