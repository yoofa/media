/*
 * aaudio_audio_track.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AAUDIO_AUDIO_TRACK_H_H_
#define AVE_MEDIA_AAUDIO_AUDIO_TRACK_H_H_

#include <aaudio/AAudio.h>

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

  status_t GetPosition(uint32_t* position) const override;
  int64_t GetPlayedOutDurationUs(int64_t nowUs) const override;
  status_t GetFramesWritten(uint32_t* frameswritten) const override;
  int64_t GetBufferDurationInUs() const override;

  status_t Open(audio_config_t config, AudioCallback cb, void* cookie) override;
  status_t Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Close() override;
  ssize_t Write(const void* buffer, size_t size, bool blocking) override;

 private:
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

  AAudioStream* stream_ = nullptr;
  bool started_ = false;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_AAUDIO_AUDIO_TRACK_H_H_ */
