/*
 * media_packet.cc
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_packet.h"
#include "base/checks.h"

namespace ave {
namespace media {

MediaPacket MediaPacket::Create(size_t size) {
  return {size};
}

MediaPacket MediaPacket::CreateWithHandle(void* handle) {
  return {handle};
}

std::shared_ptr<MediaPacket> MediaPacket::CreateShared(size_t size) {
  return make_shared_internal<MediaPacket>(size);
}

std::shared_ptr<MediaPacket> MediaPacket::CreateSharedWithHandle(void* handle) {
  return make_shared_internal<MediaPacket>(handle);
}

MediaPacket::MediaPacket(size_t size)
    : size_(size),
      data_(std::make_shared<Buffer>(size)),
      native_handle_(nullptr),
      buffer_type_(PacketBufferType::kTypeNormal),
      media_type_(MediaType::UNKNOWN),
      is_eos_(false),
      media_meta_(MediaMeta::Create(MediaType::UNKNOWN)) {}

MediaPacket::MediaPacket(void* handle)
    : size_(0),
      data_(nullptr),
      native_handle_(handle),
      buffer_type_(PacketBufferType::kTypeNativeHandle),
      media_type_(MediaType::UNKNOWN),
      is_eos_(false),
      media_meta_(MediaMeta::Create(MediaType::UNKNOWN)) {}

MediaPacket::~MediaPacket() = default;

MediaPacket::MediaPacket(const MediaPacket& other)
    : size_(other.size_),
      media_type_(other.media_type_),
      is_eos_(other.is_eos_),
      media_meta_(other.media_meta_) {
  if (other.buffer_type_ == PacketBufferType::kTypeNormal) {
    data_ = other.data_;
    native_handle_ = nullptr;
    buffer_type_ = PacketBufferType::kTypeNormal;
  } else {
    data_ = nullptr;
    native_handle_ = other.native_handle_;
    buffer_type_ = PacketBufferType::kTypeNativeHandle;
  }
}

void MediaPacket::SetMediaType(MediaType type) {
  if (media_type_ != type) {
    media_type_ = type;
    switch (media_type_) {
      case MediaType::AUDIO:
        media_meta_ = MediaMeta::Create(MediaType::AUDIO);
        break;
      case MediaType::VIDEO:
        media_meta_ = MediaMeta::Create(MediaType::VIDEO);
        break;
      default:
        break;
    }
  }
}

void MediaPacket::SetSize(size_t size) {
  AVE_DCHECK(buffer_type_ == PacketBufferType::kTypeNormal);
  AVE_DCHECK(size > 0);
  data_ = std::make_shared<Buffer>(size);
  size_ = data_->size();
}

void MediaPacket::SetData(uint8_t* data, size_t size) {
  AVE_DCHECK(buffer_type_ == PacketBufferType::kTypeNormal);
  data_ = std::make_shared<Buffer>(data, size);
  size_ = data_->size();
}

AudioSampleInfo* MediaPacket::audio_info() {
  if (media_type_ != MediaType::AUDIO) {
    return nullptr;
  }
  auto& sample_info = media_meta_.sample_info();
  return &sample_info.audio();
}

VideoSampleInfo* MediaPacket::video_info() {
  if (media_type_ != MediaType::VIDEO) {
    return nullptr;
  }
  auto& sample_info = media_meta_.sample_info();
  return &sample_info.video();
}

const uint8_t* MediaPacket::data() const {
  if (buffer_type_ == PacketBufferType::kTypeNormal) {
    return data_->data();
  }
  return nullptr;
}

}  // namespace media
}  // namespace ave
