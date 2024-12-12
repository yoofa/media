/*
 * alsa_audio_track.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef ALSA_AUDIO_TRACK_H
#define ALSA_AUDIO_TRACK_H

#include "../audio_track.h"

namespace ave {
namespace media {

class AlsaAudioTrack : public AudioTrack {
 public:
  AlsaAudioTrack();
  ~AlsaAudioTrack() override;
  // AudioTrack implementation
  bool Init() override;
  bool Start() override;
  bool Stop() override;
  bool Pause() override;
  bool Resume() override;
  bool Flush() override;
  bool SetVolume(float volume) override;
  bool SetMute(bool mute) override;
  bool SetAudioParameters(const AudioParameters& params) override;
  bool GetAudioParameters(AudioParameters* params) override;
  bool SetPlaybackRate(float rate) override;
  bool GetPlaybackRate(float* rate) override;
  bool SetPlaybackPitch(float pitch) override;
  bool GetPlaybackPitch(float* pitch) override;
  bool SetPlaybackSpeed(float speed) override;
  bool GetPlaybackSpeed(float* speed) override;
  bool SetPlaybackPosition(int64_t position) override;
  bool GetPlaybackPosition(int64_t* position) override;
  bool GetDuration(int64_t* duration) override;
  bool SetLooping(bool looping) override;
  bool IsLooping() override;
  bool SetAudioEffect(const AudioEffect& effect) override;
  bool GetAudioEffect(AudioEffect* effect) override;
  bool SetAudioEffectEnabled(bool enabled) override;
  bool IsAudioEffectEnabled() override;
  bool SetAudioEffectVolume(float volume) override;
  bool GetAudioEffectVolume(float* volume) override;
  bool SetAudioEffectSendLevel(float level) override;
  bool GetAudioEffectSendLevel(float* level) override;
  bool SetAudioEffectSendLevel(int effectId, float level) override;
  bool GetAudioEffectSendLevel(int effectId, float* level) override;
  bool SetAuxEffectSendLevel(float level) override;
  bool GetAuxEffectSendLevel(float* level) override;
  bool SetAuxEffectSendLevel(int effectId, float level) override;
  bool GetAuxEffectSendLevel(int effectId, float* level) override;
  bool SetAuxEffectEnabled(bool enabled) override;
  bool IsAuxEffectEnabled() override;
  bool SetAuxEffectVolume(float volume) override;
  bool GetAuxEffectVolume(float* volume) override;
  bool SetAuxEffectSendLevel(int effectId, float level) override;
  bool GetAuxEffectSendLevel(int effectId, float* level) override;
};

}  // namespace media
}  // namespace ave

#endif /* !ALSA_AUDIO_TRACK_H */
