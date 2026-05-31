/*
 * encoded_image_buffer.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/api/encoded_image_buffer.h"

#include <cstring>
#include <utility>

namespace ave {
namespace media {
namespace rtp_rtcp {

std::shared_ptr<EncodedImageBuffer> EncodedImageBuffer::Create(size_t size) {
  return std::shared_ptr<EncodedImageBuffer>(new EncodedImageBuffer(size));
}

std::shared_ptr<EncodedImageBuffer> EncodedImageBuffer::Create(
    const uint8_t* data,
    size_t size) {
  return std::shared_ptr<EncodedImageBuffer>(
      new EncodedImageBuffer(data, size));
}

std::shared_ptr<EncodedImageBuffer> EncodedImageBuffer::Create(
    base::Buffer buffer) {
  return std::shared_ptr<EncodedImageBuffer>(
      new EncodedImageBuffer(std::move(buffer)));
}

EncodedImageBuffer::EncodedImageBuffer(size_t size) : buffer_(size) {}

EncodedImageBuffer::EncodedImageBuffer(const uint8_t* data, size_t size)
    : buffer_(data, size) {}

EncodedImageBuffer::EncodedImageBuffer(base::Buffer buffer)
    : buffer_(std::move(buffer)) {}

const uint8_t* EncodedImageBuffer::data() const {
  return buffer_.data();
}

uint8_t* EncodedImageBuffer::data() {
  return buffer_.data();
}

size_t EncodedImageBuffer::size() const {
  return buffer_.size();
}

void EncodedImageBuffer::Realloc(size_t size) {
  buffer_.SetSize(size);
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
