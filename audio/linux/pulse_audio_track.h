/*
 * pulse_audio_track.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_LINUX_PULSE_AUDIO_TRACK_H_
#define AVE_MEDIA_AUDIO_LINUX_PULSE_AUDIO_TRACK_H_

#include <pulse/pulseaudio.h>
#include <memory>

#include "base/types.h"
#include "media/audio/audio_track.h"

namespace ave {
namespace media {
namespace linux {

class PulseAudioTrack : public AudioTrack {
 public:
  PulseAudioTrack();
  ~PulseAudioTrack() override;

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
  ssize_t Write(const void* buffer, size_t size, bool blocking) override;
  status_t Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Close() override;

 private:
  static void StreamStateCallback(pa_stream* s, void* userdata);
  static void StreamWriteCallback(pa_stream* s, size_t length, void* userdata);
  static void StreamUnderflowCallback(pa_stream* s, void* userdata);

  status_t InitPulseAudio();
  void DestroyPulseAudio();

  pa_threaded_mainloop* mainloop_;
  pa_context* context_;
  pa_stream* stream_;

  audio_config_t config_;
  AudioCallback callback_;
  void* cookie_;

  bool ready_;
  bool playing_;

  size_t buffer_size_;
  std::unique_ptr<uint8_t[]> temp_buffer_;
  size_t temp_buffer_size_;
};

}  // namespace linux
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_AUDIO_LINUX_PULSE_AUDIO_TRACK_H_