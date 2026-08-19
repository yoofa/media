/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/audio_rtp_receive_stream.h"

#include <cstring>
#include <utility>

#include "base/checks.h"
#include "base/logging.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"

namespace ave::media::rtp_session {

AudioRtpReceiveStream::AudioRtpReceiveStream(const Config& config)
    : AsyncMediaFrameSource(config.task_runner_factory),
      config_(config),
      packet_buffer_(config.packet_buffer),
      timestamp_mapper_({.clock_rate_hz = config.rtp_clock_rate_hz}) {
  AVE_CHECK_GT(config_.rtp_clock_rate_hz, 0U);
}

AudioRtpReceiveStream::~AudioRtpReceiveStream() {
  Stop();
}

void AudioRtpReceiveStream::Start() {
  running_.store(true);
}

void AudioRtpReceiveStream::Stop() {
  running_.store(false);
  std::scoped_lock lock(mutex_);
  packet_buffer_.Reset();
  timestamp_mapper_.Reset();
}

void AudioRtpReceiveStream::OnRtpPacket(
    const rtp_rtcp::RtpPacketReceived& packet) {
  if (!running_.load() || packet.Ssrc() != config_.remote_ssrc) {
    return;
  }
  if (!config_.payload_type_codecs.contains(packet.PayloadType())) {
    AVE_LOG(LS_WARNING) << "Dropping unconfigured audio payload type "
                        << static_cast<uint32_t>(packet.PayloadType());
    return;
  }

  BufferedAudioPacket buffered_packet{
      .sequence_number = packet.SequenceNumber(),
      .rtp_timestamp = packet.Timestamp(),
      .payload_type = packet.PayloadType(),
      .arrival_time = packet.arrival_time(),
      .payload = std::vector<uint8_t>(packet.payload().begin(),
                                      packet.payload().end()),
  };
  std::scoped_lock lock(mutex_);
  if (!running_.load()) {
    return;
  }
  for (BufferedAudioPacket& ready_packet :
       packet_buffer_.Insert(std::move(buffered_packet))) {
    DeliverPacket(std::move(ready_packet));
  }
}

AudioPacketBufferStats AudioRtpReceiveStream::GetPacketBufferStats() const {
  std::scoped_lock lock(mutex_);
  return packet_buffer_.GetStats();
}

void AudioRtpReceiveStream::DeliverPacket(BufferedAudioPacket packet) {
  auto frame =
      MediaFrame::CreateShared(packet.payload.size(), MediaType::AUDIO);
  std::memcpy(frame->data(), packet.payload.data(), packet.payload.size());
  frame->setRange(0, packet.payload.size());
  const base::Timestamp pts = timestamp_mapper_.Map(packet.rtp_timestamp);
  base::TimeDelta duration =
      base::TimeDelta::Millis(config_.default_frame_duration_ms);
  if (packet.duration_rtp_ticks.has_value() && *packet.duration_rtp_ticks > 0 &&
      *packet.duration_rtp_ticks <= config_.rtp_clock_rate_hz / 5) {
    duration = base::TimeDelta::Micros(
        static_cast<int64_t>(*packet.duration_rtp_ticks) * 1'000'000 /
        static_cast<int64_t>(config_.rtp_clock_rate_hz));
  }
  frame->SetCodec(config_.payload_type_codecs.at(packet.payload_type))
      .SetSampleRate(config_.rtp_clock_rate_hz)
      .SetChannelLayout(CHANNEL_LAYOUT_MONO)
      .SetPts(pts)
      .SetDts(pts)
      .SetDuration(duration);
  frame->set_arrival_time(packet.arrival_time);
  DeliverFrame(frame);
}

}  // namespace ave::media::rtp_session
