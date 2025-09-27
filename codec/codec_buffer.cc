/*
 * codec_buffer.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "codec_buffer.h"

namespace ave {

namespace media {

CodecBuffer::CodecBuffer(void* data, size_t size)
    : buffer_(std::make_shared<media::Buffer>(data, size)),
      texture_id_(-1),
      native_handle_(nullptr),
      buffer_type_(BufferType::kTypeNormal),
      format_(MediaMeta::CreatePtr()) {}

CodecBuffer::CodecBuffer(size_t capacity)
    : buffer_(std::make_shared<media::Buffer>(capacity)),
      texture_id_(-1),
      native_handle_(nullptr),
      buffer_type_(BufferType::kTypeNormal),
      format_(MediaMeta::CreatePtr()) {}

CodecBuffer::~CodecBuffer() = default;

uint8_t* CodecBuffer::base() {
  return buffer_->base();
}

uint8_t* CodecBuffer::data() {
  return buffer_->data();
}

size_t CodecBuffer::capacity() {
  return buffer_->capacity();
}

size_t CodecBuffer::size() {
  return buffer_->size();
}

size_t CodecBuffer::offset() {
  return buffer_->offset();
}

status_t CodecBuffer::SetRange(size_t offset, size_t size) {
  buffer_->setRange(offset, size);
  return OK;
}

status_t CodecBuffer::EnsureCapacity(size_t capacity, bool copy) {
  buffer_->ensureCapacity(capacity, copy);
  return OK;
}

status_t CodecBuffer::ResetBuffer(std::shared_ptr<media::Buffer>& buffer) {
  buffer_ = buffer;
  return OK;
}

}  // namespace media
}  // namespace ave
