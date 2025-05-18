/*
 * alsa_audio_record.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_ALSA_AUDIO_RECORD_H_H_
#define AVE_MEDIA_ALSA_AUDIO_RECORD_H_H_

#include <alsa/asoundlib.h>

#include <memory>

#include "media/audio/audio.h"
#include "media/audio/audio_record.h"
#include "media/audio/linux/alsa_symbol_table.h"

namespace ave {
namespace media {
namespace linux_audio {

class AlsaAudioRecord : public AudioRecord {
 public:
  explicit AlsaAudioRecord(AlsaSymbolTable* symbol_table);
  ~AlsaAudioRecord() override;

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
  status_t SetHWParams();
  status_t SetSWParams();
  status_t RecoverIfNeeded(int error);

  AlsaSymbolTable* symbol_table_;
  snd_pcm_t* handle_;
  audio_config_t config_;
  AudioCallback callback_;
  void* cookie_;

  bool ready_;
  bool recording_;

  snd_pcm_uframes_t period_size_;
  snd_pcm_uframes_t buffer_size_;
  uint64_t frames_read_;
  float gain_;
};

}  // namespace linux_audio
}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_ALSA_AUDIO_RECORD_H_H_ */
