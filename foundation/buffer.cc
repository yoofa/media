/*
 * buffer.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "buffer.h"

#include <cstring>
#include <memory>

#include "base/checks.h"

namespace ave {
namespace media {

Buffer::Buffer(size_t capacity)
    : buffer_(std::make_unique<base::Buffer>(capacity)),
      data_(buffer_->data()),
      capacity_(capacity),
      range_offset_(0),
      range_length_(capacity),
      int32_data_(0),
      owns_data_(true) {}

Buffer::Buffer(void* data, size_t capacity)
    : data_(data),
      capacity_(capacity),
      range_offset_(0),
      range_length_(capacity),
      int32_data_(0),
      owns_data_(false) {}

// static
std::shared_ptr<Buffer> Buffer::CreateAsCopy(const void* data,
                                             size_t capacity) {
  auto buffer = std::make_shared<Buffer>(capacity);
  std::memcpy(buffer->data(), data, capacity);
  return buffer;
}

Buffer::~Buffer() = default;

void Buffer::setRange(size_t offset, size_t size) {
  AVE_CHECK_LE(offset, capacity_);
  AVE_CHECK_LE(offset + size, capacity_);

  range_offset_ = offset;
  range_length_ = size;
}

void Buffer::ensureCapacity(size_t capacity, bool copy) {
  if (capacity <= capacity_) {
    return;
  }

  if (owns_data_) {
    auto new_buffer = std::make_unique<base::Buffer>(capacity);
    if (copy) {
      std::memcpy(new_buffer->data(), data_, range_length_);
    }
    buffer_ = std::move(new_buffer);
    data_ = buffer_->data();
  } else {
    AVE_CHECK(copy);
    AVE_CHECK(false);
  }
  capacity_ = capacity;
}

std::shared_ptr<Message>& Buffer::meta() {
  if (meta_ == nullptr) {
    meta_ = std::make_shared<Message>();
  }
  return meta_;
}

}  // namespace media
}  // namespace ave
