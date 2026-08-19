/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/pcm_ring_buffer.h"

#include <algorithm>
#include <cstring>

#include "base/checks.h"

namespace ave::media {

PcmRingBuffer::PcmRingBuffer(size_t capacity_bytes) : buffer_(capacity_bytes) {
  AVE_CHECK_GT(capacity_bytes, 0U);
}

size_t PcmRingBuffer::Write(std::span<const uint8_t> data) {
  if (data.empty()) {
    return 0;
  }
  const uint64_t write_position =
      write_position_.load(std::memory_order_relaxed);
  const uint64_t read_position = read_position_.load(std::memory_order_acquire);
  const auto queued = static_cast<size_t>(write_position - read_position);
  if (data.size() > buffer_.size() - queued) {
    return 0;
  }

  const auto offset = static_cast<size_t>(write_position % buffer_.size());
  const size_t first_copy = std::min(data.size(), buffer_.size() - offset);
  std::memcpy(buffer_.data() + offset, data.data(), first_copy);
  if (first_copy < data.size()) {
    std::memcpy(buffer_.data(), data.data() + first_copy,
                data.size() - first_copy);
  }
  write_position_.store(write_position + data.size(),
                        std::memory_order_release);
  return data.size();
}

size_t PcmRingBuffer::Read(std::span<uint8_t> output) {
  if (output.empty()) {
    return 0;
  }
  const uint64_t read_position = read_position_.load(std::memory_order_relaxed);
  const uint64_t write_position =
      write_position_.load(std::memory_order_acquire);
  const size_t bytes_to_read = std::min(
      output.size(), static_cast<size_t>(write_position - read_position));
  const auto offset = static_cast<size_t>(read_position % buffer_.size());
  const size_t first_copy = std::min(bytes_to_read, buffer_.size() - offset);
  std::memcpy(output.data(), buffer_.data() + offset, first_copy);
  if (first_copy < bytes_to_read) {
    std::memcpy(output.data() + first_copy, buffer_.data(),
                bytes_to_read - first_copy);
  }
  read_position_.store(read_position + bytes_to_read,
                       std::memory_order_release);
  return bytes_to_read;
}

void PcmRingBuffer::Clear() {
  const uint64_t write_position =
      write_position_.load(std::memory_order_acquire);
  read_position_.store(write_position, std::memory_order_release);
}

size_t PcmRingBuffer::queued_bytes() const {
  const uint64_t read_position = read_position_.load(std::memory_order_acquire);
  const uint64_t write_position =
      write_position_.load(std::memory_order_acquire);
  return write_position >= read_position
             ? static_cast<size_t>(write_position - read_position)
             : 0;
}

}  // namespace ave::media
