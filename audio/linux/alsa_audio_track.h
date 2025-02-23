/*
 * alsa_audio_track.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_LINUX_ALSA_AUDIO_TRACK_H_
#define AVE_MEDIA_AUDIO_LINUX_ALSA_AUDIO_TRACK_H_

#include <alsa/asoundlib.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <memory>

#include "base/types.h"
#include "media/audio/audio.h"
#include "media/audio/audio_track.h"
#include "media/audio/linux/alsa_audio_device.h"
#include "media/audio/linux/alsa_symbol_table.h"

namespace ave {
namespace media {
namespace linux_audio {

class AlsaAudioTrack : public AudioTrack {
 public:
  explicit AlsaAudioTrack(AlsaSymbolTable* symbol_table);
  ~AlsaAudioTrack() override;

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
  status_t GetFramesWritten(uint32_t* frameswritten) const override;
  int64_t GetPlayedOutDurationUs(int64_t nowUs) const override;
  int64_t GetBufferDurationInUs() const override;

  status_t Open(audio_config_t config, AudioCallback cb, void* cookie) override;
  ssize_t Write(const void* buffer, size_t size, bool blocking) override;
  status_t Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Close() override;

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
  bool playing_;

  snd_pcm_uframes_t period_size_;
  snd_pcm_uframes_t buffer_size_;
  uint64_t frames_written_;
};

}  // namespace linux_audio
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_AUDIO_LINUX_ALSA_AUDIO_TRACK_H_
