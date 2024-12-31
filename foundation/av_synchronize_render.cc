#include "av_synchronize_render.h"

#include "base/checks.h"
#include "base/logging.h"
#include "base/task_util/default_task_runner_factory.h"
#include "base/task_util/task_runner.h"
#include "base/time_utils.h"
#include "base/sequence_checker.h"

namespace ave {
namespace media {

namespace {
// Maximum allowed time backwards from anchor change.
// If larger than this threshold, it's treated as discontinuity.
const int64_t kAnchorFluctuationAllowedUs = 10000LL;

// Default video frame interval (for 30fps)
const int64_t kDefaultVideoFrameIntervalUs = 33000LL;

// Minimum audio clock update period
const int64_t kMinAudioClockUpdatePeriodUs = 20000LL;  // 20ms
}  // namespace

AVSynchronizeRender::AVSynchronizeRender()
    : sync_runner_(std::make_unique<base::TaskRunner>(
          base::CreateDefaultTaskRunnerFactory()->CreateTaskRunner(
              "AVSync",
              base::TaskRunnerFactory::Priority::NORMAL))),
      media_clock_(std::make_shared<MediaClock>()),
      clock_type_(ClockType::kDefault),
      audio_track_(nullptr),
      use_audio_callback_(false) {}

AVSynchronizeRender::~AVSynchronizeRender() {
  Flush();
}

void AVSynchronizeRender::QueueBuffer(
    int32_t stream_index,
    std::shared_ptr<MediaFrame> buffer,
    std::unique_ptr<RenderEvent> render_event) {
  sync_runner_->PostTask([this, stream_index, buffer = std::move(buffer),
                          render_event = std::move(render_event)]() {
    AVE_DCHECK_RUN_ON(sync_runner_);
    OnQueueBuffer(stream_index, std::move(buffer), std::move(render_event));
  });
}

void AVSynchronizeRender::OnQueueBuffer(
    int32_t stream_index,
    std::shared_ptr<MediaFrame> frame,
    std::unique_ptr<RenderEvent> render_event) {
  MediaType type = frame->GetMediaType();
  if (streams_.find(stream_index) == streams_.end()) {
    AVE_LOG(LS_INFO) << "stream " << stream_index << " get first frame";
    streams_[stream_index] = {type, {}};
  }
  // TODO: drop frame if stale

  if (type == MediaType::AUDIO) {
    has_audio_ = true;
  } else if (type == MediaType::VIDEO) {
    has_video_ = true;
  }

  streams_[stream_index].second.push(
      QueueEntry{frame, std::move(render_event)});

  if (type == MediaType::AUDIO) {
    PostDrainAudioQueue(stream_index);
  } else if (type == MediaType::VIDEO) {
    PostDrainVideoQueue(stream_index);
  }

  // TODO: sync queues
}

void AVSynchronizeRender::PostDrainAudioQueue(int32_t stream_index) {
  if (use_audio_callback_) {
    return;
  }

  auto& queue = streams_[stream_index].second;
  if (queue.empty()) {
    return;
  }
}

void AVSynchronizeRender::OnDrainAudioQueue(int32_t stream_index) {}

void AVSynchronizeRender::PostDrainVideoQueue(int32_t stream_index) {
  if (paused_ && !video_sample_received_) {
    return;
  }

  auto& queue = streams_[stream_index].second;
  if (queue.empty()) {
    return;
  }
  auto& entry = queue.front();
  bool is_eos = entry.frame->video_info()->eos;
  if (is_eos) {
    OnDrainVideoQueue(stream_index);
    return;
  }

  auto pts_us = entry.frame->video_info()->pts.us();
  if (anchor_time_media_us_ < 0) {
    media_clock_->UpdateAnchor(pts_us, base::TimeMicros(), pts_us);
    anchor_time_media_us_ = pts_us;
  }

  if (!has_audio_) {
    media_clock_->UpdateMaxTimeMedia(pts_us + kDefaultVideoFrameIntervalUs);
  }

  if (!video_sample_received_ || pts_us < audio_first_anchor_time_media_us_) {
    OnDrainVideoQueue(stream_index);
  } else {
    media_clock_->AddTimerEvent(
        [this, stream_index]() {
          sync_runner_->PostTask(
              [this, stream_index]() { OnDrainVideoQueue(stream_index); });
        },
        pts_us, -2 * video_render_delay_us_);
  }
}

void AVSynchronizeRender::OnDrainVideoQueue(int32_t stream_index) {
  auto& queue = streams_[stream_index].second;
  if (queue.empty()) {
    return;
  }
  auto& entry = queue.front();
  bool is_eos = entry.frame->video_info()->eos;
  if (is_eos) {
    // TODO: notify eos
    queue.pop();
    return;
  }

  auto pts_us = entry.frame->video_info()->pts.us();
  int64_t now_us = base::TimeMicros();
  int64_t real_time_us = media_clock_->GetRealTimeFor(pts_us, now_us);
}

void AVSynchronizeRender::Flush() {
  sync_runner_->PostTask([this]() {
    for (auto& stream : streams_) {
      stream.second.queue.clear();
    }
    media_clock_->ClearAnchor();
  });

}

void AVSynchronizeRender::Pause() {
  if (audio_track_) {
    audio_track_->Pause();
  }
}

void AVSynchronizeRender::Resume() {
  if (audio_track_) {
    audio_track_->Start();
  }
}

status_t AVSynchronizeRender::GetCurrentMediaTime(int64_t* out_media_time_us) {
  auto ret = media_clock_->GetMediaTime(base::TimeMicros(), out_media_time_us);
  if (ret == OK) {
    return ret;
  }
  // media clock not started, try to start it if possible
  {

  }
  return media_clock_->GetMediaTime(base::TimeMicros(), out_media_time_us);
}

void AVSynchronizeRender::SetVideoFrameRate(float fps) {
  // Update video scheduling parameters based on frame rate
  auto frame_duration_us = static_cast<int64_t>(1000000.0f / fps);
  video_frame_interval_us_ = frame_duration_us;
}

void AVSynchronizeRender::SetMasterClock(ClockType type,
                                         int32_t master_stream_index) {
  sync_runner_->PostTask([this, type, master_stream_index]() {
    clock_type_ = type;
    master_stream_index_ = master_stream_index;
  });
}

void AVSynchronizeRender::SetAudioTrack(
    std::shared_ptr<AudioTrack> audio_track) {
  audio_track_ = audio_track;
}

void AVSynchronizeRender::DrainAudioQueue() {

}

void AVSynchronizeRender::DrainVideoQueue() {

}

}  // namespace media
}  // namespace ave
