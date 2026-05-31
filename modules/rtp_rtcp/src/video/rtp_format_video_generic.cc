/*
 * rtp_format_video_generic.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/video/rtp_format_video_generic.h"
#include <span>

#include <string.h>

#include <optional>

#include "base/checks.h"
#include "base/logging.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace {

constexpr size_t kGenericHeaderLength = 1;
// Extended header length is 2 bytes (not used in simplified implementation).
// constexpr size_t kExtendedHeaderLength = 2;

}  // namespace

RtpPacketizerGeneric::RtpPacketizerGeneric(
    std::span<const uint8_t> payload,
    PayloadSizeLimits limits,
    const RTPVideoHeader& rtp_video_header)
    : remaining_payload_(payload) {
  BuildHeader(rtp_video_header);

  limits.max_payload_len -= header_size_;
  payload_sizes_ = SplitAboutEqually(payload.size(), limits);
  current_packet_ = payload_sizes_.begin();
}

RtpPacketizerGeneric::RtpPacketizerGeneric(std::span<const uint8_t> payload,
                                           PayloadSizeLimits limits)
    : header_size_(0), remaining_payload_(payload) {
  payload_sizes_ = SplitAboutEqually(payload.size(), limits);
  current_packet_ = payload_sizes_.begin();
}

RtpPacketizerGeneric::~RtpPacketizerGeneric() = default;

size_t RtpPacketizerGeneric::NumPackets() const {
  return payload_sizes_.end() - current_packet_;
}

bool RtpPacketizerGeneric::NextPacket(RtpPacketToSend* packet) {
  AVE_DCHECK(packet);
  if (current_packet_ == payload_sizes_.end())
    return false;

  size_t next_packet_payload_len = *current_packet_;

  uint8_t* out_ptr =
      packet->AllocatePayload(header_size_ + next_packet_payload_len);
  AVE_CHECK(out_ptr);

  if (header_size_ > 0) {
    memcpy(out_ptr, header_, header_size_);
    // Remove first-packet bit, following packets are intermediate.
    header_[0] &= ~RtpFormatVideoGeneric::kFirstPacketBit;
  }

  memcpy(out_ptr + header_size_, remaining_payload_.data(),
         next_packet_payload_len);

  remaining_payload_ = remaining_payload_.subspan(next_packet_payload_len);

  ++current_packet_;

  // Packets left to produce and data left to split should end at the same time.
  AVE_DCHECK_EQ(current_packet_ == payload_sizes_.end(),
                remaining_payload_.empty());

  packet->SetMarker(remaining_payload_.empty());
  return true;
}

void RtpPacketizerGeneric::BuildHeader(const RTPVideoHeader& rtp_video_header) {
  header_size_ = kGenericHeaderLength;
  header_[0] = RtpFormatVideoGeneric::kFirstPacketBit;
  if (rtp_video_header.frame_type == VideoFrameType::kVideoFrameKey) {
    header_[0] |= RtpFormatVideoGeneric::kKeyFrameBit;
  }
  // Note: Extended header with picture_id is not supported in our simplified
  // implementation.
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
