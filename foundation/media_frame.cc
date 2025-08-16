/*
 * media_packet.cc
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
  return std::allocate_shared<MediaFrame>(std::allocator<MediaFrame>(), size,
                                          media_type);
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

void MediaFrame::SetSize(size_t size) {
  AVE_DCHECK(buffer_type_ == FrameBufferType::kTypeNormal);
  AVE_DCHECK(size > 0);
  data_ = std::make_shared<Buffer>(size);
}

void MediaFrame::SetData(uint8_t* data, size_t size) {
  AVE_DCHECK(buffer_type_ == FrameBufferType::kTypeNormal);
  data_ = std::make_shared<Buffer>(data, size);
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

const uint8_t* MediaFrame::data() const {
  if (buffer_type_ == FrameBufferType::kTypeNormal) {
    return data_ == nullptr ? nullptr : data_->data();
  }
  return nullptr;
}

}  // namespace media
}  // namespace ave
