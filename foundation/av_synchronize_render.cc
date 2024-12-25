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

void AVSynchronizeRender::QueueBuffer(int32_t stream_index,
                                      std::shared_ptr<MediaFrame> buffer,
                                      std::function<void()> consume_notify) {
  QueueEntry entry;
  entry.frame = std::move(buffer);
  entry.consumed_notify = std::move(consume_notify);

  sync_runner_->PostTask([this, stream_index, entry = std::move(entry)]() {
    AVE_DCHECK_RUN_ON(sync_runner_);

    media_clock_->AddTimerEvent([](){}, );
  });
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
