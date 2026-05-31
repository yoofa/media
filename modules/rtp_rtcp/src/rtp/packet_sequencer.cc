/*
 * packet_sequencer.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/rtp/packet_sequencer.h"

#include <random>
#include "base/checks.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace {
// RED header is first byte of payload, if present.
constexpr size_t kRedForFecHeaderLength = 1;

// Timestamps use a 90kHz clock.
constexpr uint32_t kTimestampTicksPerMs = 90;
}  // namespace

PacketSequencer::PacketSequencer(uint32_t media_ssrc,
                                 std::optional<uint32_t> rtx_ssrc,
                                 bool require_marker_before_media_padding,
                                 base::Clock* clock)
    : media_ssrc_(media_ssrc),
      rtx_ssrc_(rtx_ssrc),
      require_marker_before_media_padding_(require_marker_before_media_padding),
      clock_(clock),
      media_sequence_number_(0),
      rtx_sequence_number_(0),
      last_payload_type_(-1),
      last_rtp_timestamp_(0),
      last_packet_marker_bit_(false) {
  std::mt19937 gen(clock_->TimeInMicroseconds());
  // Random start, 16 bits. Upper half of range is avoided in order to prevent
  // SRTP wraparound issues during startup.
  // Sequence number 0 is avoided for historical reasons.
  constexpr uint16_t kMaxInitRtpSeqNumber = 0x7fff;  // 2^15 - 1.
  std::uniform_int_distribution<uint16_t> dis(1, kMaxInitRtpSeqNumber);
  media_sequence_number_ = dis(gen);
  rtx_sequence_number_ = dis(gen);
}

void PacketSequencer::Sequence(RtpPacketToSend& packet) {
  if (packet.Ssrc() == media_ssrc_) {
    if (packet.packet_type() == RtpPacketMediaType::kRetransmission) {
      // Retransmission of an already sequenced packet, ignore.
      return;
    } else if (packet.packet_type() == RtpPacketMediaType::kPadding) {
      PopulatePaddingFields(packet);
    }
    packet.SetSequenceNumber(media_sequence_number_++);
    if (packet.packet_type() != RtpPacketMediaType::kPadding) {
      UpdateLastPacketState(packet);
    }
  } else if (packet.Ssrc() == rtx_ssrc_) {
    if (packet.packet_type() == RtpPacketMediaType::kPadding) {
      PopulatePaddingFields(packet);
    }
    packet.SetSequenceNumber(rtx_sequence_number_++);
  } else {
    AVE_NOTREACHED() << "Unexpected ssrc " << packet.Ssrc();
  }
}

void PacketSequencer::SetRtpState(const RtpState& state) {
  media_sequence_number_ = state.sequence_number;
  last_rtp_timestamp_ = state.timestamp;
  last_capture_time_ = state.capture_time;
  last_timestamp_time_ = state.last_timestamp_time;
}

void PacketSequencer::PopulateRtpState(RtpState& state) const {
  state.sequence_number = media_sequence_number_;
  state.timestamp = last_rtp_timestamp_;
  state.capture_time = last_capture_time_;
  state.last_timestamp_time = last_timestamp_time_;
}

void PacketSequencer::UpdateLastPacketState(const RtpPacketToSend& packet) {
  // Remember marker bit to determine if padding can be inserted with
  // sequence number following `packet`.
  last_packet_marker_bit_ = packet.Marker();
  // Remember media payload type to use in the padding packet if rtx is
  // disabled.
  if (packet.is_red()) {
    AVE_DCHECK_GE(packet.payload_size(), kRedForFecHeaderLength);
    last_payload_type_ = packet.PayloadBuffer()[0];
  } else {
    last_payload_type_ = packet.PayloadType();
  }
  // Save timestamps to generate timestamp field and extensions for the padding.
  last_rtp_timestamp_ = packet.Timestamp();
  last_timestamp_time_ = clock_->CurrentTime();
  last_capture_time_ = packet.capture_time();
}

void PacketSequencer::PopulatePaddingFields(RtpPacketToSend& packet) {
  if (packet.Ssrc() == media_ssrc_) {
    AVE_DCHECK(CanSendPaddingOnMediaSsrc());

    packet.SetTimestamp(last_rtp_timestamp_);
    packet.set_capture_time(last_capture_time_);
    packet.SetPayloadType(last_payload_type_);
    return;
  }

  AVE_DCHECK(packet.Ssrc() == rtx_ssrc_);
  if (packet.payload_size() > 0) {
    // This is payload padding packet, don't update timestamp fields.
    return;
  }

  packet.SetTimestamp(last_rtp_timestamp_);
  packet.set_capture_time(last_capture_time_);

  // Only change the timestamp of padding packets sent over RTX.
  // Padding only packets over RTP has to be sent as part of a media
  // frame (and therefore the same timestamp).
  if (last_timestamp_time_ > base::Timestamp::Zero()) {
    base::TimeDelta since_last_media =
        clock_->CurrentTime() - last_timestamp_time_;
    packet.SetTimestamp(packet.Timestamp() +
                        since_last_media.ms() * kTimestampTicksPerMs);
    if (packet.capture_time() > base::Timestamp::Zero()) {
      packet.set_capture_time(packet.capture_time() + since_last_media);
    }
  }
}

bool PacketSequencer::CanSendPaddingOnMediaSsrc() const {
  if (last_payload_type_ == -1) {
    return false;
  }

  // Without RTX we can't send padding in the middle of frames.
  // For audio marker bits doesn't mark the end of a frame and frames
  // are usually a single packet, so for now we don't apply this rule
  // for audio.
  if (require_marker_before_media_padding_ && !last_packet_marker_bit_) {
    return false;
  }

  return true;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
