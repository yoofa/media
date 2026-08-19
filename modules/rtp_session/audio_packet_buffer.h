/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_SESSION_AUDIO_PACKET_BUFFER_H_
#define MEDIA_MODULES_RTP_SESSION_AUDIO_PACKET_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "base/numerics/sequence_number_unwrapper.h"
#include "base/units/timestamp.h"

namespace ave::media::rtp_session {

struct BufferedAudioPacket {
  uint16_t sequence_number = 0;
  uint32_t rtp_timestamp = 0;
  uint8_t payload_type = 0;
  base::Timestamp arrival_time = base::Timestamp::MinusInfinity();
  std::optional<uint32_t> duration_rtp_ticks;
  std::vector<uint8_t> payload;
};

struct AudioPacketBufferStats {
  uint64_t inserted_packets = 0;
  uint64_t released_packets = 0;
  uint64_t duplicate_packets = 0;
  uint64_t late_packets = 0;
  uint64_t skipped_packets = 0;
  size_t buffered_packets = 0;
};

// A deliberately small reorder buffer for interactive audio. It retains one
// future packet by default and skips a missing sequence number once enough
// newer data is available, preventing loss from growing playout latency.
class AudioPacketBuffer {
 public:
  struct Config {
    size_t target_delay_packets = 1;
    size_t max_buffered_packets = 4;
  };

  AudioPacketBuffer();
  explicit AudioPacketBuffer(Config config);

  std::vector<BufferedAudioPacket> Insert(BufferedAudioPacket packet);
  void Reset();
  AudioPacketBufferStats GetStats() const;

 private:
  int64_t Unwrap(uint16_t sequence_number);
  std::vector<BufferedAudioPacket> ReleaseReadyPackets();

  Config config_;
  base::RtpSequenceNumberUnwrapper sequence_unwrapper_;
  std::optional<int64_t> newest_sequence_number_;
  std::optional<int64_t> next_sequence_number_;
  bool released_any_packet_ = false;
  std::map<int64_t, BufferedAudioPacket> packets_;
  AudioPacketBufferStats stats_;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_AUDIO_PACKET_BUFFER_H_
