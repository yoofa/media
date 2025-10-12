#ifndef MEDIA_AUDIO_ANDROID_OPENSLES_AUDIO_TRACK_H_
#define MEDIA_AUDIO_ANDROID_OPENSLES_AUDIO_TRACK_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <memory>
#include <vector>

#include "media/audio/audio_track.h"

namespace ave {
namespace media {
namespace android {

class OpenSLESAudioTrack : public AudioTrack {
 public:
  OpenSLESAudioTrack(SLEngineItf engine, SLObjectItf output_mix);
  ~OpenSLESAudioTrack() override;

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
  static void BufferQueueCallback(SLAndroidSimpleBufferQueueItf caller,
                                  void* context);
  status_t CreatePlayer(const AudioConfig& config);
  void DestroyPlayer();
  void OnBufferComplete();

  SLEngineItf engine_engine_;
  SLObjectItf output_mix_object_;

  // Player interfaces
  SLObjectItf player_object_ = nullptr;
  SLPlayItf player_play_ = nullptr;
  SLAndroidSimpleBufferQueueItf player_buffer_queue_ = nullptr;

  AudioConfig config_;
  AudioCallback callback_ = nullptr;
  void* cookie_ = nullptr;
  bool is_playing_ = false;

  // Callback mode support
  static constexpr size_t kNumBuffers = 2;
  std::vector<std::unique_ptr<uint8_t[]>> callback_buffers_;
  size_t buffer_size_ = 0;
  size_t current_buffer_index_ = 0;
};

}  // namespace android
}  // namespace media
}  // namespace ave

#endif  // MEDIA_AUDIO_ANDROID_OPENSLES_AUDIO_TRACK_H_
