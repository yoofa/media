/*
 * av_synchronize_render.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AV_SYNCHRONIZE_RENDER_H
#define AV_SYNCHRONIZE_RENDER_H

#include <functional>
#include <list>
#include <memory>

#include "base/task_util/task_runner.h"
#include "media/audio/audio_track.h"
#include "media_clock.h"
#include "media_frame.h"
#include "media_sink_base.h"
#include "media_utils.h"

namespace ave {
namespace media {

struct RenderEvent {
  virtual ~RenderEvent() = default;
  // rendered is true if the audio frame is rendered to sink or video frame is
  // too late rendered is false if the video frame need be rendered by source
  virtual void OnRenderEvent(bool rendered) = 0;
};

namespace synchronizer_impl {

template <typename Closure>
class ClosureEvent : public RenderEvent {
 public:
  explicit ClosureEvent(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}

 private:
  void OnRenderEvent(bool rendered) override { closure_(rendered); }
  std::decay_t<Closure> closure_;
};

template <typename Closure>
std::unique_ptr<RenderEvent> ToRenderEvent(Closure&& closure) {
  return std::make_unique<synchronizer_impl::ClosureEvent<Closure>>(
      std::forward<Closure>(closure));
}

}  // namespace synchronizer_impl

// I have 2 design options:
// 1. avp decoder -> av_synchronize_render  ->
// 1.1 render coupling with decoder  -> avp decoder -> VideoRender & AudioRender
// 1.2 not coupling                  -> VideoRender & AudioRender
// 2. avp decoder -> avplayer -> av_synchronize_render
// and then the same as 1.1 and 1.2
//
// use the 1rd design now
class AVSynchronizeRender : public MessageObject {
 public:
  enum class ClockType : uint8_t {
    kSystem,
    kAudio,
    kDefault = kAudio,
  };

  AVSynchronizeRender();
  ~AVSynchronizeRender() override;

  // no need MediaType param any more, media type can get from MediaFrame
  void QueueBuffer(int32_t stream_index,
                   std::shared_ptr<MediaFrame> frame,
                   std::unique_ptr<RenderEvent> render_event = nullptr);

  template <
      class Closure,
      std::enable_if_t<
          !std::is_convertible_v<Closure, std::unique_ptr<RenderEvent>>>* =
          nullptr>
  void QueueBuffer(int32_t stream_index,
                   std::shared_ptr<MediaFrame> frame,
                   Closure&& closure) {
    QueueBuffer(stream_index, std::move(frame),
                synchronizer_impl::ToRenderEvent(closure));
  }

  void QueueEOS(int32_t stream_index, status_t final_result = OK);

  void Flush();

  void Pause();
  void Resume();

  void SetVideoFrameRate(float fps);

  status_t SetPlaybackRate(float rate);
  status_t GetPlaybackRate(float* rate);

  // if type is kSystem, master_audio_stream_index is ignored
  // if type is kAudio, master_audio_stream_index must be a audio stream index
  // and the audio stream will be the master clock
  void SetMasterClock(ClockType type, int32_t master_audio_stream_index = -1);

  // track not opened when set, only support 1 audio sink now.
  // multi audio stream will be mix to 1 audio stream to this audio_track\
  // deprecated, use OpenAudioSink instead, because audio sink open and write
  // or other operation must be in the same thread
  void SetAudioTrack(std::shared_ptr<AudioTrack> audio_track);
  // TODO: support multi audio track, each audio stream can be play to different
  // audio track

  status_t OpenAudioSink(const std::shared_ptr<MediaMeta>& format,
                         bool has_video = false,
                         bool offload_only = false,
                         bool* is_offloaded = nullptr,
                         bool is_streaming = false);

 private:
  struct QueueEntry {
    std::shared_ptr<MediaFrame> frame;
    std::unique_ptr<RenderEvent> render_event;
  };
  using StreamQueue = std::pair<MediaType, std::queue<QueueEntry>>;

  void OnQueueBuffer(int32_t stream_index,
                     std::shared_ptr<MediaFrame> frame,
                     std::unique_ptr<RenderEvent> render_event);

  void PostDrainAudioQueue(int32_t stream_index);

  void PostDrainVideoQueue(int32_t stream_index);

  void OnDrainAudioQueue(int32_t stream_index);

  void OnDrainVideoQueue(int32_t stream_index);

  /************** data **************/

  std::unique_ptr<base::TaskRunner> sync_runner_;
  std::mutex mutex_;

  // streams
  std::unordered_map<int32_t, StreamQueue> streams_;

  std::shared_ptr<MediaClock> media_clock_;
  ClockType clock_type_;

  std::shared_ptr<AudioTrack> audio_track_;
  bool use_audio_callback_;

  float playback_rate_;

  int64_t video_render_delay_us_;
  int64_t anchor_time_media_us_;
  int64_t audio_first_anchor_time_media_us_;
  int64_t video_frame_interval_us_;
  bool has_audio_;
  bool has_video_;
  bool paused_;
  bool video_sample_received_;
};

}  // namespace media
}  // namespace ave

#endif /* !AV_SYNCHRONIZE_RENDER_H */
