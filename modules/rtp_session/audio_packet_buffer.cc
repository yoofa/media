/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/audio_packet_buffer.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/checks.h"

namespace ave::media::rtp_session {

AudioPacketBuffer::AudioPacketBuffer() : AudioPacketBuffer(Config{}) {}

AudioPacketBuffer::AudioPacketBuffer(Config config) : config_(config) {
  AVE_CHECK_GT(config_.max_buffered_packets, config_.target_delay_packets);
}

std::vector<BufferedAudioPacket> AudioPacketBuffer::Insert(
    BufferedAudioPacket packet) {
  const int64_t extended_sequence_number = Unwrap(packet.sequence_number);
  if (next_sequence_number_.has_value() &&
      extended_sequence_number < *next_sequence_number_) {
    ++stats_.late_packets;
    return {};
  }
  if (packets_.contains(extended_sequence_number)) {
    ++stats_.duplicate_packets;
    return {};
  }

  packets_.emplace(extended_sequence_number, std::move(packet));
  ++stats_.inserted_packets;
  if (!released_any_packet_) {
    next_sequence_number_ = packets_.begin()->first;
  }
  return ReleaseReadyPackets();
}

void AudioPacketBuffer::Reset() {
  sequence_unwrapper_.Reset();
  newest_sequence_number_.reset();
  next_sequence_number_.reset();
  released_any_packet_ = false;
  packets_.clear();
  stats_ = {};
}

AudioPacketBufferStats AudioPacketBuffer::GetStats() const {
  AudioPacketBufferStats stats = stats_;
  stats.buffered_packets = packets_.size();
  return stats;
}

int64_t AudioPacketBuffer::Unwrap(uint16_t sequence_number) {
  if (!newest_sequence_number_.has_value()) {
    const int64_t unwrapped = sequence_unwrapper_.Unwrap(sequence_number);
    newest_sequence_number_ = unwrapped;
    return unwrapped;
  }
  const int64_t unwrapped = sequence_unwrapper_.PeekUnwrap(sequence_number);
  if (unwrapped > *newest_sequence_number_) {
    newest_sequence_number_ = sequence_unwrapper_.Unwrap(sequence_number);
    return *newest_sequence_number_;
  }
  return unwrapped;
}

std::vector<BufferedAudioPacket> AudioPacketBuffer::ReleaseReadyPackets() {
  std::vector<BufferedAudioPacket> released;
  while (packets_.size() > config_.target_delay_packets) {
    AVE_DCHECK(next_sequence_number_.has_value());
    auto packet_it = packets_.find(*next_sequence_number_);
    if (packet_it == packets_.end()) {
      const int64_t first_buffered_sequence = packets_.begin()->first;
      if (first_buffered_sequence > *next_sequence_number_) {
        stats_.skipped_packets += static_cast<uint64_t>(
            first_buffered_sequence - *next_sequence_number_);
        next_sequence_number_ = first_buffered_sequence;
        packet_it = packets_.begin();
      }
    }
    if (packet_it == packets_.end()) {
      break;
    }
    const auto next_packet_it = std::next(packet_it);
    if (next_packet_it != packets_.end() &&
        next_packet_it->first == packet_it->first + 1) {
      packet_it->second.duration_rtp_ticks =
          next_packet_it->second.rtp_timestamp -
          packet_it->second.rtp_timestamp;
    }
    released.push_back(std::move(packet_it->second));
    packets_.erase(packet_it);
    ++(*next_sequence_number_);
    released_any_packet_ = true;
    ++stats_.released_packets;
  }

  while (packets_.size() > config_.max_buffered_packets) {
    stats_.skipped_packets += static_cast<uint64_t>(
        packets_.begin()->first -
        next_sequence_number_.value_or(packets_.begin()->first));
    next_sequence_number_ = packets_.begin()->first;
    released.push_back(std::move(packets_.begin()->second));
    packets_.erase(packets_.begin());
    ++(*next_sequence_number_);
    released_any_packet_ = true;
    ++stats_.released_packets;
  }
  return released;
}

}  // namespace ave::media::rtp_session
