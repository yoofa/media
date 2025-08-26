/*
 * media_frame.cc
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_frame.h"
#include "base/checks.h"

namespace ave {
namespace media {

MediaFrame MediaFrame::Create(size_t size, MediaType media_type) {
  return {size, media_type};
}

std::shared_ptr<MediaFrame> MediaFrame::CreateShared(size_t size,
                                                     MediaType media_type) {
  return std::make_shared<MediaFrame>(size, media_type);
}

std::shared_ptr<MediaFrame>
MediaFrame::CreateSharedAsCopy(void* data, size_t size, MediaType media_type) {
  auto frame = std::make_shared<MediaFrame>(size, media_type);
  std::memcpy(frame->data(), data, size);
  return frame;
}

MediaFrame::MediaFrame(size_t size, MediaType media_type)
    : MediaMeta(media_type),
      data_(size == 0 ? nullptr : std::make_shared<media::Buffer>(size)),
      buffer_type_(FrameBufferType::kTypeNormal),
      native_handle_(nullptr) {}

MediaFrame::MediaFrame(const MediaFrame& other) : MediaMeta(other) {
  if (other.buffer_type_ == FrameBufferType::kTypeNormal) {
    data_ = other.data_;
    native_handle_ = nullptr;
    buffer_type_ = FrameBufferType::kTypeNormal;
  } else {
    data_ = nullptr;
    native_handle_ = other.native_handle_;
    buffer_type_ = FrameBufferType::kTypeNativeHandle;
  }
}

void MediaFrame::setRange(size_t offset, size_t size) {
  if (data_) {
    data_->setRange(offset, size);
  }
}

void MediaFrame::ensureCapacity(size_t capacity, bool copy) {
  if (data_) {
    data_->ensureCapacity(capacity, copy);
  } else {
    data_ = std::make_shared<media::Buffer>(capacity);
  }
}

AudioSampleInfo* MediaFrame::audio_info() {
  auto* sample_info = this->sample_info();
  if (sample_info == nullptr) {
    return nullptr;
  }
  return &sample_info->audio();
}

VideoSampleInfo* MediaFrame::video_info() {
  auto* sample_info = this->sample_info();
  if (sample_info == nullptr) {
    return nullptr;
  }
  return &sample_info->video();
}

uint8_t* MediaFrame::data() const {
  if (buffer_type_ == FrameBufferType::kTypeNormal) {
    return data_ == nullptr ? nullptr : data_->data();
  }
  return nullptr;
}

}  // namespace media
}  // namespace ave
