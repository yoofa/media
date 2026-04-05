/*
 * java_audio_track.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_JAVA_AUDIO_TRACK_H
#define AVE_MEDIA_JAVA_AUDIO_TRACK_H

#include <jni.h>

#include "media/audio/audio_track.h"
#include "third_party/jni_zero/java_refs.h"

namespace ave {
namespace media {
namespace android {

/**
 * AudioTrack implementation that delegates to a Java AudioSink via JNI.
 *
 * Lifecycle:
 *  1. Constructed with a Java AudioSink global reference.
 *  2. Open() → calls AudioSink.open(sampleRate, channels, encoding)
 *  3. Write() → calls AudioSink.write(ByteBuffer, size)
 *  4. Close() → calls AudioSink.close()
 */
class JavaAudioTrack : public AudioTrack {
 public:
  // Takes ownership of a global reference to a Java AudioSink object.
  explicit JavaAudioTrack(
      const jni_zero::ScopedJavaGlobalRef<jobject>& j_audio_sink);
  ~JavaAudioTrack() override;

  // AudioTrack interface
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
  // Convert native audio_format_t to AudioSink encoding constant.
  static int ToAudioSinkEncoding(audio_format_t format);

  jni_zero::ScopedJavaGlobalRef<jobject> j_audio_sink_;
  AudioConfig config_{};
  bool opened_ = false;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_JAVA_AUDIO_TRACK_H
