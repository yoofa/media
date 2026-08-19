/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/video_rtp_receive_stream.h"

#include <algorithm>
#include <cstring>
#include <span>
#include <utility>

#include "base/logging.h"
#include "media/codec/codec_id.h"
#include "media/foundation/h264/h264_common.h"
#include "media/foundation/video_frame_type.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"

namespace ave::media::rtp_session {
namespace {

constexpr uint8_t kNalTypeMask = 0x1f;
constexpr uint8_t kFuStartBit = 0x80;
constexpr uint8_t kFuEndBit = 0x40;
constexpr uint8_t kStapAType = 24;
constexpr uint8_t kFuAType = 28;
constexpr uint8_t kIdrType = 5;
constexpr uint8_t kSpsType = 7;
constexpr uint8_t kPpsType = 8;
constexpr uint8_t kAudType = 9;
constexpr std::array<uint8_t, 4> kAnnexBStartCode = {0, 0, 0, 1};

void AppendStartCode(std::vector<uint8_t>* output) {
  output->insert(output->end(), kAnnexBStartCode.begin(),
                 kAnnexBStartCode.end());
}

bool IsKeyNal(uint8_t nal_type) {
  return nal_type == kIdrType || nal_type == kSpsType;
}

}  // namespace

VideoRtpReceiveStream::VideoRtpReceiveStream(const Config& config)
    : AsyncMediaFrameSource(config.task_runner_factory), config_(config) {}

VideoRtpReceiveStream::~VideoRtpReceiveStream() {
  Stop();
}

void VideoRtpReceiveStream::Start() {
  running_.store(true);
}

void VideoRtpReceiveStream::Stop() {
  running_.store(false);
  std::scoped_lock lock(mutex_);
  pending_frame_.reset();
  sequence_unwrapper_.Reset();
  timestamp_unwrapper_.Reset();
  first_rtp_timestamp_.reset();
}

void VideoRtpReceiveStream::OnRtpPacket(
    const rtp_rtcp::RtpPacketReceived& packet) {
  if (!running_.load() || packet.Ssrc() != config_.remote_ssrc ||
      packet.PayloadType() != config_.payload_type) {
    return;
  }

  std::optional<PacketFragment> fragment = ParsePacket(packet);
  std::shared_ptr<MediaFrame> completed_frame;
  {
    std::scoped_lock lock(mutex_);
    ++stats_.packets_received;
    if (!fragment.has_value()) {
      ++stats_.invalid_packets_dropped;
      return;
    }

    if (!pending_frame_.has_value() ||
        pending_frame_->rtp_timestamp != packet.Timestamp()) {
      if (pending_frame_.has_value() && !pending_frame_->packets.empty()) {
        ++stats_.incomplete_frames_dropped;
      }
      ResetPendingFrame(packet.Timestamp(), packet.arrival_time());
    }

    const int64_t sequence =
        sequence_unwrapper_.Unwrap(packet.SequenceNumber());
    if (pending_frame_->packets.contains(sequence)) {
      return;
    }
    if (pending_frame_->packets.size() >= config_.max_packets_per_frame) {
      ++stats_.incomplete_frames_dropped;
      ResetPendingFrame(packet.Timestamp(), packet.arrival_time());
    }

    if (fragment->starts_frame) {
      pending_frame_->first_sequence = sequence;
    }
    if (!pending_frame_->first_sequence.has_value()) {
      pending_frame_->first_sequence = sequence;
    } else {
      pending_frame_->first_sequence =
          std::min(*pending_frame_->first_sequence, sequence);
    }
    if (fragment->marker) {
      pending_frame_->marker_sequence = sequence;
    }
    pending_frame_->packets.emplace(sequence, std::move(*fragment));
    completed_frame = TryAssembleFrame();
    if (completed_frame != nullptr) {
      ++stats_.frames_delivered;
      pending_frame_.reset();
    }
  }
  if (completed_frame != nullptr) {
    DeliverFrame(completed_frame);
  }
}

VideoRtpReceiveStream::Stats VideoRtpReceiveStream::GetStats() const {
  std::scoped_lock lock(mutex_);
  return stats_;
}

std::optional<VideoRtpReceiveStream::PacketFragment>
VideoRtpReceiveStream::ParsePacket(
    const rtp_rtcp::RtpPacketReceived& packet) {
  const std::span<const uint8_t> payload = packet.payload();
  if (payload.empty()) {
    return std::nullopt;
  }
  const uint8_t nal_type = payload[0] & kNalTypeMask;
  PacketFragment fragment{
      .sequence_number = packet.SequenceNumber(),
      .marker = packet.Marker(),
      .starts_frame = nal_type == kSpsType || nal_type == kPpsType ||
                      nal_type == kAudType,
      .key_frame = IsKeyNal(nal_type),
      .payload = std::vector<uint8_t>(payload.begin(), payload.end()),
  };
  if (nal_type == kFuAType) {
    if (payload.size() < 3) {
      return std::nullopt;
    }
    const uint8_t original_type = payload[1] & kNalTypeMask;
    fragment.starts_frame = (payload[1] & kFuStartBit) != 0;
    fragment.key_frame = IsKeyNal(original_type);
    if (packet.Marker() && (payload[1] & kFuEndBit) == 0) {
      return std::nullopt;
    }
  } else if (nal_type == kStapAType) {
    if (payload.size() < 4) {
      return std::nullopt;
    }
    fragment.starts_frame = true;
  } else if (nal_type == 0 || nal_type >= kStapAType) {
    return std::nullopt;
  }
  return fragment;
}

std::shared_ptr<MediaFrame> VideoRtpReceiveStream::TryAssembleFrame() {
  if (!pending_frame_->first_sequence.has_value() ||
      !pending_frame_->marker_sequence.has_value() ||
      *pending_frame_->first_sequence > *pending_frame_->marker_sequence) {
    return nullptr;
  }
  for (int64_t sequence = *pending_frame_->first_sequence;
       sequence <= *pending_frame_->marker_sequence; ++sequence) {
    if (!pending_frame_->packets.contains(sequence)) {
      return nullptr;
    }
  }

  const PacketFragment& first_fragment =
      pending_frame_->packets.at(*pending_frame_->first_sequence);
  const std::span<const uint8_t> first_payload(first_fragment.payload);
  if ((first_payload[0] & kNalTypeMask) == kFuAType &&
      (first_payload[1] & kFuStartBit) == 0) {
    return nullptr;
  }

  std::vector<uint8_t> annex_b;
  bool key_frame = false;
  for (int64_t sequence = *pending_frame_->first_sequence;
       sequence <= *pending_frame_->marker_sequence; ++sequence) {
    const PacketFragment& fragment = pending_frame_->packets.at(sequence);
    const std::span<const uint8_t> payload(fragment.payload);
    const uint8_t nal_type = payload[0] & kNalTypeMask;
    key_frame = key_frame || fragment.key_frame;
    if (nal_type == kStapAType) {
      size_t offset = 1;
      while (offset + 2 <= payload.size()) {
        const size_t nal_size =
            (static_cast<size_t>(payload[offset]) << 8) | payload[offset + 1];
        offset += 2;
        if (nal_size == 0 || offset + nal_size > payload.size()) {
          ++stats_.invalid_packets_dropped;
          return nullptr;
        }
        AppendStartCode(&annex_b);
        const std::span<const uint8_t> nal = payload.subspan(offset, nal_size);
        annex_b.insert(annex_b.end(), nal.begin(), nal.end());
        key_frame = key_frame || IsKeyNal(payload[offset] & kNalTypeMask);
        offset += nal_size;
      }
      if (offset != payload.size()) {
        ++stats_.invalid_packets_dropped;
        return nullptr;
      }
      continue;
    }
    if (nal_type == kFuAType) {
      const bool starts_nal = (payload[1] & kFuStartBit) != 0;
      if (starts_nal) {
        AppendStartCode(&annex_b);
        annex_b.push_back((payload[0] & 0xe0) | (payload[1] & kNalTypeMask));
      }
      annex_b.insert(annex_b.end(), payload.begin() + 2, payload.end());
      continue;
    }
    AppendStartCode(&annex_b);
    annex_b.insert(annex_b.end(), payload.begin(), payload.end());
  }
  if (annex_b.empty()) {
    return nullptr;
  }

  auto frame = MediaFrame::CreateShared(annex_b.size(), MediaType::VIDEO);
  std::memcpy(frame->data(), annex_b.data(), annex_b.size());
  frame->setRange(0, annex_b.size());
  const base::Timestamp pts = MapRtpTimestamp(pending_frame_->rtp_timestamp);
  frame->SetCodec(CodecId::AVE_CODEC_ID_H264)
      .SetPictureType(key_frame ? PictureType::I : PictureType::P)
      .SetPts(pts)
      .SetDts(pts);
  frame->set_arrival_time(pending_frame_->arrival_time);
  return frame;
}

void VideoRtpReceiveStream::ResetPendingFrame(
    uint32_t rtp_timestamp,
    base::Timestamp arrival_time) {
  pending_frame_ = PendingFrame{
      .rtp_timestamp = rtp_timestamp,
      .arrival_time = arrival_time,
  };
}

base::Timestamp VideoRtpReceiveStream::MapRtpTimestamp(
    uint32_t rtp_timestamp) {
  const int64_t unwrapped = timestamp_unwrapper_.Unwrap(rtp_timestamp);
  if (!first_rtp_timestamp_.has_value()) {
    first_rtp_timestamp_ = unwrapped;
  }
  const int64_t relative_ticks = unwrapped - *first_rtp_timestamp_;
  const auto clock_rate = static_cast<int64_t>(config_.rtp_clock_rate_hz);
  return base::Timestamp::Micros(relative_ticks / clock_rate * 1'000'000 +
                                 relative_ticks % clock_rate * 1'000'000 /
                                     clock_rate);
}

}  // namespace ave::media::rtp_session
