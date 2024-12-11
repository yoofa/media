/*
 * codec_buffer.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "codec_buffer.h"

namespace ave {

namespace media {

CodecBuffer::CodecBuffer() : CodecBuffer(0) {}

CodecBuffer::CodecBuffer(size_t capacity)
    : buffer_(std::make_shared<Buffer>(capacity)),
      buffer_index_(-1),
      texture_id_(-1),
      native_handle_(nullptr),
      buffer_type_(BufferType::kTypeNormal),
      format_(std::make_shared<MediaFormat>()) {}

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

void CodecBuffer::SetRange(size_t offset, size_t size) {
  buffer_->setRange(offset, size);
}

void CodecBuffer::EnsureCapacity(size_t capacity, bool copy) {
  buffer_->ensureCapacity(capacity, copy);
}

void CodecBuffer::ResetBuffer(std::shared_ptr<Buffer>& buffer) {
  buffer_ = buffer;
}

}  // namespace media
}  // namespace ave
