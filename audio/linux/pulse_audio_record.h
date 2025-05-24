/*
 * pulse_audio_record.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_LINUX_PULSE_AUDIO_RECORD_H_
#define AVE_MEDIA_AUDIO_LINUX_PULSE_AUDIO_RECORD_H_

#include <pulse/pulseaudio.h>
#include <memory>

#include "media/audio/audio_record.h"

namespace ave {
namespace media {
namespace linux_audio {

class PulseAudioRecord : public AudioRecord {
 public:
  PulseAudioRecord();
  ~PulseAudioRecord() override;

  // AudioRecord implementation
  bool ready() const override;
  ssize_t bufferSize() const override;
  ssize_t frameCount() const override;
  ssize_t channelCount() const override;
  ssize_t frameSize() const override;
  uint32_t sampleRate() const override;
  uint32_t latency() const override;

  status_t GetPosition(uint32_t* position) const override;
  int64_t GetRecordedDurationUs(int64_t nowUs) const override;
  status_t GetFramesRead(uint32_t* frames_read) const override;

  status_t Open(audio_config_t config, AudioCallback cb, void* cookie) override;
  ssize_t Read(void* buffer, size_t size, bool blocking) override;
  status_t Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Close() override;

  status_t SetGain(float gain) override;
  float GetGain() const override;

 private:
  static void ContextStateCallback(pa_context* c, void* userdata);
  static void StreamStateCallback(pa_stream* s, void* userdata);
  static void StreamReadCallback(pa_stream* s, size_t length, void* userdata);
  static void StreamOverflowCallback(pa_stream* s, void* userdata);

  status_t InitPulseAudio();
  void DestroyPulseAudio();

  pa_threaded_mainloop* mainloop_ = nullptr;
  pa_context* context_ = nullptr;
  pa_stream* stream_ = nullptr;

  audio_config_t config_{};
  AudioCallback callback_;
  void* cookie_ = nullptr;

  bool ready_ = false;
  bool recording_ = false;

  size_t buffer_size_ = 0;
  uint64_t frames_read_ = 0;
  float gain_ = 1.0f;
};

}  // namespace linux_audio
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_AUDIO_LINUX_PULSE_AUDIO_RECORD_H_
