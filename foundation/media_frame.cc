/*
 * media_frame.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_frame.h"

#include "base/checks.h"
#include "media/foundation/media_utils.h"

namespace ave {
namespace media {

MediaFrame MediaFrame::Create(size_t size) {
  return {size, protect_parameter()};
}

MediaFrame MediaFrame::CreateWithHandle(void* handle) {
  return {handle, protect_parameter()};
}

MediaFrame::MediaFrame(size_t size, protect_parameter)
    : size_(size),
      data_(std::make_shared<Buffer>(size)),
      native_handle_(nullptr),
      buffer_type_(FrameBufferType::kTypeNormal),
      media_type_(MediaType::UNKNOWN),
      sample_info_(AudioSampleInfo{}) {}

MediaFrame::MediaFrame(void* handle, protect_parameter)
    : size_(0),
      data_(nullptr),
      native_handle_(handle),
      buffer_type_(FrameBufferType::kTypeNativeHandle),
      media_type_(MediaType::UNKNOWN),
      sample_info_(AudioSampleInfo{}) {}

MediaFrame::~MediaFrame() = default;

MediaFrame::MediaFrame(const MediaFrame& other) {
  if (other.buffer_type_ == FrameBufferType::kTypeNormal) {
    data_ = other.data_;
    native_handle_ = nullptr;
    buffer_type_ = FrameBufferType::kTypeNormal;
  } else {
    data_ = nullptr;
    native_handle_ = other.native_handle_;
    buffer_type_ = FrameBufferType::kTypeNativeHandle;
  }

  size_ = other.size_;
  media_type_ = other.media_type_;
  sample_info_ = other.sample_info_;
}

void MediaFrame::SetMediaType(MediaType type) {
  if (media_type_ != type) {
    media_type_ = type;
    switch (media_type_) {
      case MediaType::AUDIO:
        sample_info_ = AudioSampleInfo();
        break;
      case MediaType::VIDEO:
        sample_info_ = VideoSampleInfo();
        break;
      default:
        break;
    }
  }
}

void MediaFrame::SetSize(size_t size) {
  AVE_DCHECK(buffer_type_ == FrameBufferType::kTypeNormal);
  AVE_DCHECK(size > 0);
  data_ = std::make_shared<Buffer>(size);
  size_ = data_->size();
}

void MediaFrame::SetData(uint8_t* data, size_t size) {
  AVE_DCHECK(buffer_type_ == FrameBufferType::kTypeNormal);
  data_ = std::make_shared<Buffer>(data, size);
  size_ = data_->size();
}

AudioSampleInfo* MediaFrame::audio_info() {
  return std::get_if<AudioSampleInfo>(&sample_info_);
}

VideoSampleInfo* MediaFrame::video_info() {
  return std::get_if<VideoSampleInfo>(&sample_info_);
}

const uint8_t* MediaFrame::data() const {
  if (buffer_type_ == FrameBufferType::kTypeNormal) {
    return data_->data();
  }
  return nullptr;
}

}  // namespace media
}  // namespace ave
