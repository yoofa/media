/*
 * packet_source.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 *
 * Ported from Android MPEG2-TS module (AnotherPacketSource)
 * Original Copyright (C) 2010 The Android Open Source Project
 */

#include "packet_source.h"

#include "base/logging.h"
#include "foundation/media_defs.h"
#include "foundation/media_frame.h"
#include "foundation/media_meta.h"

namespace ave {
namespace media {
namespace mpeg2ts {

PacketSource::PacketSource(std::shared_ptr<MediaMeta> meta)
    : is_audio_(false),
      is_video_(false),
      enabled_(true),
      format_(nullptr),
      last_queued_time_us_(0),
      eos_result_(OK),
      latest_enqueued_meta_(nullptr),
      latest_dequeued_meta_(nullptr) {
  SetFormat(meta);
  discontinuity_segments_.push_back(DiscontinuitySegment());
}

PacketSource::~PacketSource() {}

void PacketSource::SetFormat(std::shared_ptr<MediaMeta> meta) {
  is_audio_ = false;
  is_video_ = false;

  if (meta == nullptr) {
    return;
  }

  format_ = meta;

  MediaType stream_type = meta->stream_type();
  if (stream_type == MediaType::AUDIO) {
    is_audio_ = true;
  } else if (stream_type == MediaType::VIDEO) {
    is_video_ = true;
  }
}

status_t PacketSource::Start(std::shared_ptr<Message> /* params */) {
  return OK;
}

status_t PacketSource::Stop() {
  return OK;
}

std::shared_ptr<MediaMeta> PacketSource::GetFormat() {
  std::lock_guard<std::mutex> lock(lock_);
  if (format_ != nullptr) {
    return format_;
  }

  // Search for format in buffered frames
  for (auto& entry : queue_) {
    if (!entry.IsDiscontinuity() && entry.frame_ && entry.frame_->size() > 0) {
      // Frames with data should have format information
      SetFormat(std::make_shared<MediaMeta>(*entry.frame_));
      return format_;
    }
  }
  return nullptr;
}

status_t PacketSource::DequeueAccessUnit(std::shared_ptr<MediaFrame>& frame) {
  frame = nullptr;

  std::unique_lock<std::mutex> lock(lock_);
  while (eos_result_ == OK && queue_.empty()) {
    condition_.wait(lock);
  }

  if (!queue_.empty()) {
    QueueEntry entry = queue_.front();
    queue_.pop_front();
    frame = entry.frame_;

    if (entry.IsDiscontinuity()) {
      if (WasFormatChange(static_cast<int32_t>(entry.discontinuity_type_))) {
        format_ = nullptr;
      }

      discontinuity_segments_.pop_front();
      return INFO_DISCONTINUITY;
    }

    DiscontinuitySegment& seg = discontinuity_segments_.front();

    int64_t time_us = frame->pts().us_or(-1);
    latest_dequeued_meta_ = std::make_shared<MediaMeta>(*frame);
    if (time_us > seg.max_deque_time_us_) {
      seg.max_deque_time_us_ = time_us;
    }

    return OK;
  }

  return eos_result_;
}

status_t PacketSource::Read(std::shared_ptr<MediaFrame>& frame,
                            const ReadOptions* /* options */) {
  return DequeueAccessUnit(frame);
}

bool PacketSource::WasFormatChange(int32_t discontinuity_type) const {
  if (is_audio_) {
    return (discontinuity_type &
            static_cast<int32_t>(DiscontinuityType::AUDIO_FORMAT)) != 0;
  }

  if (is_video_) {
    return (discontinuity_type &
            static_cast<int32_t>(DiscontinuityType::VIDEO_FORMAT)) != 0;
  }

  return false;
}

void PacketSource::QueueAccessUnit(std::shared_ptr<MediaFrame> frame) {
  if (!frame) {
    return;
  }

  std::lock_guard<std::mutex> lock(lock_);
  queue_.push_back(QueueEntry{.frame_ = frame});
  condition_.notify_one();

  int64_t last_queued_time_us = frame->pts().us_or(-1);
  if (last_queued_time_us >= 0) {
    last_queued_time_us_ = last_queued_time_us;

    DiscontinuitySegment& tail_seg = discontinuity_segments_.back();
    if (last_queued_time_us > tail_seg.max_enque_time_us_) {
      tail_seg.max_enque_time_us_ = last_queued_time_us;
    }
    if (tail_seg.max_deque_time_us_ < 0) {
      tail_seg.max_deque_time_us_ = last_queued_time_us;
    }
  }

  latest_enqueued_meta_ = std::make_shared<MediaMeta>(*frame);
}

void PacketSource::QueueDiscontinuity(DiscontinuityType type,
                                      std::shared_ptr<Message> extra,
                                      bool discard) {
  std::lock_guard<std::mutex> lock(lock_);

  if (discard) {
    // Clear all frames
    queue_.clear();

    for (auto& seg : discontinuity_segments_) {
      seg.Clear();
    }
  }

  eos_result_ = OK;
  last_queued_time_us_ = 0;
  latest_enqueued_meta_ = nullptr;

  if (type == DiscontinuityType::NONE) {
    return;
  }

  discontinuity_segments_.push_back(DiscontinuitySegment());

  AVE_LOG(LS_INFO) << "Queueing a discontinuity";

  auto frame = MediaFrame::CreateShared(
      0, is_video_ ? MediaType::VIDEO : MediaType::AUDIO);
  queue_.push_back(QueueEntry{
      .frame_ = frame,
      .discontinuity_type_ = type,
      .extra_ = std::move(extra),
  });
  condition_.notify_one();
}

void PacketSource::SignalEOS(status_t result) {
  if (result == OK) {
    AVE_LOG(LS_ERROR) << "SignalEOS: result must not be OK";
    return;
  }

  std::lock_guard<std::mutex> lock(lock_);
  eos_result_ = result;
  condition_.notify_one();
}

bool PacketSource::HasBufferAvailable(status_t* final_result) {
  std::lock_guard<std::mutex> lock(lock_);
  *final_result = OK;
  if (!enabled_) {
    return false;
  }
  if (!queue_.empty()) {
    return true;
  }

  *final_result = eos_result_;
  return false;
}

bool PacketSource::HasDataBufferAvailable(status_t* final_result) {
  std::lock_guard<std::mutex> lock(lock_);
  *final_result = OK;
  if (!enabled_) {
    return false;
  }

  for (const auto& entry : queue_) {
    if (!entry.IsDiscontinuity() && entry.frame_ && entry.frame_->size() > 0) {
      return true;
    }
  }

  *final_result = eos_result_;
  return false;
}

size_t PacketSource::GetAvailableBufferCount(status_t* final_result) {
  std::lock_guard<std::mutex> lock(lock_);

  *final_result = OK;
  if (!enabled_) {
    return 0;
  }
  if (!queue_.empty()) {
    return queue_.size();
  }
  *final_result = eos_result_;
  return 0;
}

int64_t PacketSource::GetBufferedDurationUs(status_t* final_result) {
  std::lock_guard<std::mutex> lock(lock_);
  *final_result = eos_result_;

  int64_t duration_us = 0;
  for (const auto& seg : discontinuity_segments_) {
    duration_us += (seg.max_enque_time_us_ - seg.max_deque_time_us_);
  }

  return duration_us;
}

status_t PacketSource::NextBufferTime(int64_t* time_us) {
  *time_us = 0;

  std::lock_guard<std::mutex> lock(lock_);

  if (queue_.empty()) {
    return eos_result_ != OK ? eos_result_ : ERROR_IO;
  }

  std::shared_ptr<MediaFrame> frame = queue_.front().frame_;
  *time_us = frame->pts().us_or(-1);

  return OK;
}

bool PacketSource::IsFinished(int64_t duration) const {
  constexpr int64_t kNearEOSMarkUs = 2000000LL;  // 2 secs

  if (duration > 0) {
    int64_t diff = duration - last_queued_time_us_;
    if (diff < kNearEOSMarkUs && diff > -kNearEOSMarkUs) {
      return true;
    }
  }
  return (eos_result_ != OK);
}

std::shared_ptr<MediaMeta> PacketSource::GetLatestEnqueuedMeta() {
  std::lock_guard<std::mutex> lock(lock_);
  return latest_enqueued_meta_;
}

std::shared_ptr<MediaMeta> PacketSource::GetLatestDequeuedMeta() {
  std::lock_guard<std::mutex> lock(lock_);
  return latest_dequeued_meta_;
}

void PacketSource::Enable(bool enable) {
  std::lock_guard<std::mutex> lock(lock_);
  enabled_ = enable;
}

void PacketSource::Clear() {
  std::lock_guard<std::mutex> lock(lock_);
  queue_.clear();
  format_ = nullptr;
  last_queued_time_us_ = 0;
  eos_result_ = OK;
  latest_enqueued_meta_ = nullptr;
  latest_dequeued_meta_ = nullptr;
  discontinuity_segments_.clear();
  discontinuity_segments_.push_back(DiscontinuitySegment());
}

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave
