/*
 * media_buffer.cc
 * Copyright (C) 2021 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_buffer.h"

namespace ave {
namespace media {

MediaBuffer::MediaBuffer(std::shared_ptr<base::Buffer> buffer,
                         std::shared_ptr<Message> meta)
    : meta_(std::move(meta)), buffer_(std::move(buffer)) {}

uint8_t* MediaBuffer::base() {
  return buffer_->base();
}

uint8_t* MediaBuffer::data() {
  return buffer_->data();
}
size_t MediaBuffer::capacity() const {
  return buffer_->capacity();
}

size_t MediaBuffer::size() const {
  return buffer_->size();
}

size_t MediaBuffer::offset() const {
  return buffer_->offset();
}

std::shared_ptr<Message> MediaBuffer::meta() {
  return meta_;
}

void MediaBuffer::setRange(size_t offset, size_t size) {
  buffer_->setRange(offset, size);
}

void MediaBuffer::setMeta(std::shared_ptr<Message> meta) {
  meta_->clear();
  meta_ = std::move(meta);
}

}  // namespace media
}  // namespace ave
