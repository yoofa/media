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

#include "base/task_util/task_runner.h"
#include "base/thread_annotation.h"
#include "media/audio/audio_track.h"
#include "media_clock.h"
#include "media_frame.h"
#include "media_utils.h"

namespace ave {
namespace media {

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
                   std::shared_ptr<MediaFrame> buffer,
                   std::function<void()> consume_notify = nullptr);

  void QueueEos(int32_t stream_index);

  void Flush();

  void Pause();
  void Resume();

  status_t GetCurrentMediaTime(int64_t* out_media_time_us);

  void SetVideoFrameRate(float fps);

  // if type is kSystem, master_stream_index is ignored
  // if type is kAudio, master_stream_index must be a audio stream index
  // and the audio stream will be the master clock
  void SetMasterClock(ClockType type, int32_t master_stream_index = -1);

  // track not opened when set
  void SetAudioTrack(std::shared_ptr<AudioTrack> audio_track);

 private:
  // struct QueueEntry {
  //   std::shared_ptr<MediaFrame> frame;
  //   std::function<void()> consumed_notify;
  // };
  struct Stream {
    MediaType type;

  };

  void DrainAudioQueue() ;
  void DrainVideoQueue();

  std::unique_ptr<base::TaskRunner> sync_runner_;

  std::unordered_map<int32_t, Stream> streams_ GUARDED_BY(sync_runner_);
  std::shared_ptr<MediaClock> media_clock_ GUARDED_BY(sync_runner_);
  ClockType clock_type_ GUARDED_BY(sync_runner_);
  int32_t master_stream_index_ GUARDED_BY(sync_runner_);

  std::shared_ptr<AudioTrack> audio_track_;
  bool use_audio_callback_;

  int64_t video_frame_interval_us_;
};

}  // namespace media
}  // namespace ave

#endif /* !AV_SYNCHRONIZE_RENDER_H */
