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
  return MediaPacket(size, protect_parameter());
}

MediaPacket MediaPacket::CreateWithHandle(void* handle) {
  return MediaPacket(handle, protect_parameter());
}

MediaPacket::MediaPacket(size_t size, protect_parameter)
    : size_(size),
      data_(std::make_shared<Buffer>(size)),
      native_handle_(nullptr),
      buffer_type_(PacketBufferType::kTypeNormal),
      media_type_(MediaType::UNKNOWN),
      is_eos_(false),
      media_format_(MediaFormat::Create(MediaType::UNKNOWN)) {}

MediaPacket::MediaPacket(void* handle, protect_parameter)
    : size_(0),
      data_(nullptr),
      native_handle_(handle),
      buffer_type_(PacketBufferType::kTypeNativeHandle),
      media_type_(MediaType::UNKNOWN),
      is_eos_(false),
      media_format_(MediaFormat::Create(MediaType::UNKNOWN)) {}

MediaPacket::~MediaPacket() = default;

MediaPacket::MediaPacket(const MediaPacket& other)
    : size_(other.size_),
      media_type_(other.media_type_),
      is_eos_(other.is_eos_),
      media_format_(other.media_format_) {
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
        media_format_ = MediaFormat::Create(MediaType::AUDIO);
        break;
      case MediaType::VIDEO:
        media_format_ = MediaFormat::Create(MediaType::VIDEO);
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
  auto& sample_info = media_format_.sample_info();
  return &sample_info.audio();
}

VideoSampleInfo* MediaPacket::video_info() {
  if (media_type_ != MediaType::VIDEO) {
    return nullptr;
  }
  auto& sample_info = media_format_.sample_info();
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
