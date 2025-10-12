#include "media/audio/android/opensles_audio_track.h"

#include "base/logging.h"

namespace ave {
namespace media {
namespace android {

namespace {

SLuint32 GetSLSampleRate(int sample_rate) {
  switch (sample_rate) {
    case 8000:
      return SL_SAMPLINGRATE_8;
    case 11025:
      return SL_SAMPLINGRATE_11_025;
    case 16000:
      return SL_SAMPLINGRATE_16;
    case 22050:
      return SL_SAMPLINGRATE_22_05;
    case 24000:
      return SL_SAMPLINGRATE_24;
    case 32000:
      return SL_SAMPLINGRATE_32;
    case 44100:
      return SL_SAMPLINGRATE_44_1;
    case 48000:
      return SL_SAMPLINGRATE_48;
    default:
      return 0;
  }
}

SLuint32 GetSLBitsPerSample(AudioFormat format) {
  switch (format) {
    case AudioFormat::AUDIO_FORMAT_PCM_8_BIT:
      return SL_PCMSAMPLEFORMAT_FIXED_8;
    case AudioFormat::AUDIO_FORMAT_PCM_16_BIT:
      return SL_PCMSAMPLEFORMAT_FIXED_16;
    case AudioFormat::AUDIO_FORMAT_PCM_32_BIT:
      return SL_PCMSAMPLEFORMAT_FIXED_32;
    default:
      return 0;
  }
}

}  // namespace

OpenSLESAudioTrack::OpenSLESAudioTrack(SLEngineItf engine,
                                       SLObjectItf output_mix)
    : engine_engine_(engine), output_mix_object_(output_mix) {}

OpenSLESAudioTrack::~OpenSLESAudioTrack() {
  Stop();
  DestroyPlayer();
}

bool OpenSLESAudioTrack::ready() const {
  return player_object_ != nullptr;
}

ssize_t OpenSLESAudioTrack::bufferSize() const {
  return 0;
}

ssize_t OpenSLESAudioTrack::frameCount() const {
  return 0;
}

ssize_t OpenSLESAudioTrack::channelCount() const {
  return 0;
}

ssize_t OpenSLESAudioTrack::frameSize() const {
  return 0;
}

uint32_t OpenSLESAudioTrack::sampleRate() const {
  return 0;
}

uint32_t OpenSLESAudioTrack::latency() const {
  return 0;
}

float OpenSLESAudioTrack::msecsPerFrame() const {
  return 0;
}

status_t OpenSLESAudioTrack::GetPosition(uint32_t* position) const {
  return 0;
}

int64_t OpenSLESAudioTrack::GetPlayedOutDurationUs(int64_t nowUs) const {
  return 0;
}

status_t OpenSLESAudioTrack::GetFramesWritten(uint32_t* frameswritten) const {
  return 0;
}

int64_t OpenSLESAudioTrack::GetBufferDurationInUs() const {
  return 0;
}

status_t OpenSLESAudioTrack::Open(audio_config_t config,
                                  AudioCallback cb,
                                  void* cookie) {
  config_ = config;
  callback_ = cb;
  cookie_ = cookie;

  status_t result = CreatePlayer(config);
  if (result != OK) {
    return result;
  }

  // Initialize callback buffers if callback mode is enabled
  if (callback_) {
    // Calculate buffer size: 10ms of audio data
    auto channel_count = ChannelLayoutToChannelCount(config.channel_layout);
    size_t bytes_per_sample = 2;  // Assuming 16-bit PCM
    buffer_size_ =
        (config.sample_rate / 100) * channel_count * bytes_per_sample;

    callback_buffers_.resize(kNumBuffers);
    for (size_t i = 0; i < kNumBuffers; ++i) {
      callback_buffers_[i].reset(new uint8_t[buffer_size_]);
    }
    current_buffer_index_ = 0;
  }

  return OK;
}

status_t OpenSLESAudioTrack::CreatePlayer(const AudioConfig& config) {
  SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

  auto channel_count = ChannelLayoutToChannelCount(config.channel_layout);

  SLDataFormat_PCM format_pcm = {
      SL_DATAFORMAT_PCM,
      static_cast<SLuint32>(channel_count),
      GetSLSampleRate(config.sample_rate),
      GetSLBitsPerSample(config.format),
      GetSLBitsPerSample(config.format),
      channel_count == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
                         : SL_SPEAKER_FRONT_CENTER,
      SL_BYTEORDER_LITTLEENDIAN};

  SLDataSource audio_src = {&loc_bufq, &format_pcm};

  SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX,
                                        output_mix_object_};
  SLDataSink audio_sink = {&loc_outmix, nullptr};

  const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
  const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

  SLresult result =
      (*engine_engine_)
          ->CreateAudioPlayer(engine_engine_, &player_object_, &audio_src,
                              &audio_sink, 2, ids, req);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to create OpenSL ES audio player: " << result;
    return INVALID_OPERATION;
  }

  result = (*player_object_)->Realize(player_object_, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to realize OpenSL ES audio player: " << result;
    return INVALID_OPERATION;
  }

  result = (*player_object_)
               ->GetInterface(player_object_, SL_IID_PLAY, &player_play_);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to get OpenSL ES player interface: " << result;
    return INVALID_OPERATION;
  }

  result = (*player_object_)
               ->GetInterface(player_object_, SL_IID_BUFFERQUEUE,
                              &player_buffer_queue_);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to get OpenSL ES buffer queue interface: "
                      << result;
    return INVALID_OPERATION;
  }

  result =
      (*player_buffer_queue_)
          ->RegisterCallback(player_buffer_queue_, BufferQueueCallback, this);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to register OpenSL ES buffer callback: "
                      << result;
    return INVALID_OPERATION;
  }

  return OK;
}

void OpenSLESAudioTrack::DestroyPlayer() {
  if (player_object_) {
    (*player_object_)->Destroy(player_object_);
    player_object_ = nullptr;
    player_play_ = nullptr;
    player_buffer_queue_ = nullptr;
  }
}

status_t OpenSLESAudioTrack::Start() {
  if (!player_play_) {
    return INVALID_OPERATION;
  }

  SLresult result =
      (*player_play_)->SetPlayState(player_play_, SL_PLAYSTATE_PLAYING);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to start OpenSL ES playback: " << result;
    return INVALID_OPERATION;
  }

  is_playing_ = true;

  // Prime the buffer queue in callback mode
  if (callback_ && callback_buffers_.size() > 0) {
    for (size_t i = 0; i < kNumBuffers; ++i) {
      OnBufferComplete();
    }
  }

  return OK;
}

void OpenSLESAudioTrack::Stop() {
  if (!player_play_) {
    return;
  }

  SLresult result =
      (*player_play_)->SetPlayState(player_play_, SL_PLAYSTATE_STOPPED);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to stop OpenSL ES playback: " << result;
    return;
  }

  is_playing_ = false;
}

void OpenSLESAudioTrack::Pause() {
  if (!player_play_) {
    return;
  }

  SLresult result =
      (*player_play_)->SetPlayState(player_play_, SL_PLAYSTATE_PAUSED);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to pause OpenSL ES playback: " << result;
    return;
  }

  is_playing_ = false;
}

void OpenSLESAudioTrack::Close() {
  if (!player_object_) {
    return;
  }

  DestroyPlayer();

  // Clean up callback buffers
  callback_buffers_.clear();
  buffer_size_ = 0;
  current_buffer_index_ = 0;
  callback_ = nullptr;
  cookie_ = nullptr;
}

void OpenSLESAudioTrack::Flush() {
  if (!player_buffer_queue_) {
    return;
  }

  SLresult result = (*player_buffer_queue_)->Clear(player_buffer_queue_);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_ERROR) << "Failed to flush OpenSL ES buffer queue: " << result;
    return;
  }
}

ssize_t OpenSLESAudioTrack::Write(const void* buffer,
                                  size_t size,
                                  bool blocking) {
  if (!player_buffer_queue_ || !is_playing_) {
    return 0;
  }

  SLresult result =
      (*player_buffer_queue_)->Enqueue(player_buffer_queue_, buffer, size);
  if (result != SL_RESULT_SUCCESS) {
    // AVE_LOG(LS_ERROR) << "Failed to enqueue data to OpenSL ES buffer queue: "
    //                   << result;
    return 0;
  }

  return size;
}

void OpenSLESAudioTrack::BufferQueueCallback(
    SLAndroidSimpleBufferQueueItf caller,
    void* context) {
  // This callback is called when a buffer has finished playing
  OpenSLESAudioTrack* track = static_cast<OpenSLESAudioTrack*>(context);
  if (track && track->callback_) {
    track->OnBufferComplete();
  }
}

void OpenSLESAudioTrack::OnBufferComplete() {
  if (!callback_ || !is_playing_ || callback_buffers_.empty()) {
    return;
  }

  // Get the next buffer
  uint8_t* buffer = callback_buffers_[current_buffer_index_].get();

  // Request data from callback
  callback_(this, buffer, buffer_size_, cookie_, CB_EVENT_FILL_BUFFER);

  // Enqueue the filled buffer
  SLresult result = (*player_buffer_queue_)
                        ->Enqueue(player_buffer_queue_, buffer, buffer_size_);
  if (result != SL_RESULT_SUCCESS) {
    AVE_LOG(LS_WARNING) << "Failed to enqueue buffer in callback mode: "
                        << result;
    return;
  }

  // Move to next buffer
  current_buffer_index_ = (current_buffer_index_ + 1) % kNumBuffers;
}

}  // namespace android
}  // namespace media
}  // namespace ave
