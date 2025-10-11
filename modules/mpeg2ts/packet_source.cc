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
  if (format_ != nullptr) {
    // Only allowed to be set once. Requires explicit clear to reset.
    return;
  }

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
  for (auto& frame : frames_) {
    // Check if this is not a discontinuity marker
    if (frame->size() > 0) {
      // Frames with data should have format information
      SetFormat(std::make_shared<MediaMeta>(*frame));
      return format_;
    }
  }
  return nullptr;
}

status_t PacketSource::DequeueAccessUnit(std::shared_ptr<MediaFrame>& frame) {
  frame = nullptr;

  std::unique_lock<std::mutex> lock(lock_);
  while (eos_result_ == OK && frames_.empty()) {
    condition_.wait(lock);
  }

  if (!frames_.empty()) {
    frame = frames_.front();
    frames_.pop_front();

    // Check if this is a discontinuity marker (empty frame with discontinuity flag)
    int32_t discontinuity_type = 0;
    bool is_discontinuity = false;
    
    // Use frame's built-in meta to check discontinuity
    if (frame->size() == 0) {
      // This might be a discontinuity marker
      is_discontinuity = true;
      discontinuity_type = static_cast<int32_t>(DiscontinuityType::TIME);
    }

    if (is_discontinuity) {
      if (WasFormatChange(discontinuity_type)) {
        format_ = nullptr;
      }

      discontinuity_segments_.pop_front();
      return INFO_DISCONTINUITY;
    }

    DiscontinuitySegment& seg = discontinuity_segments_.front();

    int64_t time_us = frame->pts().us();
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
  frames_.push_back(frame);
  condition_.notify_one();

  // Check if this is a discontinuity marker
  if (frame->size() == 0) {
    LOG(INFO) << "Queueing a discontinuity";

    last_queued_time_us_ = 0;
    eos_result_ = OK;
    latest_enqueued_meta_ = nullptr;

    discontinuity_segments_.push_back(DiscontinuitySegment());
    return;
  }

  int64_t last_queued_time_us = frame->pts().us();
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
    frames_.clear();

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

  // Create a discontinuity marker (empty frame)
  auto frame = MediaFrame::CreateShared(
      0, is_video_ ? MediaType::VIDEO : MediaType::AUDIO);

  frames_.push_back(frame);
  condition_.notify_one();
}

void PacketSource::SignalEOS(status_t result) {
  if (result == OK) {
    LOG(ERROR) << "SignalEOS: result must not be OK";
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
  if (!frames_.empty()) {
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

  for (const auto& frame : frames_) {
    if (frame->size() > 0) {
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
  if (!frames_.empty()) {
    return frames_.size();
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

  if (frames_.empty()) {
    return eos_result_ != OK ? eos_result_ : ERROR_IO;
  }

  std::shared_ptr<MediaFrame> frame = frames_.front();
  *time_us = frame->pts().us();

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
  frames_.clear();
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
