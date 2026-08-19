/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_AUDIO_PCM_RING_BUFFER_H_
#define MEDIA_AUDIO_PCM_RING_BUFFER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ave::media {

// Bounded single-producer/single-consumer byte ring. Writes are all-or-nothing
// so the producer never modifies the consumer cursor and the audio callback
// remains lock-free.
class PcmRingBuffer {
 public:
  explicit PcmRingBuffer(size_t capacity_bytes);

  size_t Write(std::span<const uint8_t> data);
  size_t Read(std::span<uint8_t> output);
  void Clear();

  size_t capacity() const { return buffer_.size(); }
  size_t queued_bytes() const;

 private:
  std::vector<uint8_t> buffer_;
  std::atomic<uint64_t> read_position_{0};
  std::atomic<uint64_t> write_position_{0};
};

}  // namespace ave::media

#endif  // MEDIA_AUDIO_PCM_RING_BUFFER_H_
