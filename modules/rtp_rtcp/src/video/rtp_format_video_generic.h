/*
 * rtp_format_video_generic.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_

#include <stdint.h>

#include <vector>

#include <span>
#include "media/modules/rtp_rtcp/src/video/rtp_format.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpPacketToSend;

namespace RtpFormatVideoGeneric {
inline constexpr uint8_t kKeyFrameBit = 0x01;
inline constexpr uint8_t kFirstPacketBit = 0x02;
// If this bit is set, there will be an extended header contained in this
// packet. This was added later so old clients will not send this.
inline constexpr uint8_t kExtendedHeaderBit = 0x04;
}  // namespace RtpFormatVideoGeneric

class RtpPacketizerGeneric : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded generic frame.
  // Packets returned by `NextPacket` will contain the generic payload header.
  RtpPacketizerGeneric(std::span<const uint8_t> payload,
                       PayloadSizeLimits limits,
                       const RTPVideoHeader& rtp_video_header);
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded generic frame.
  // Packets returned by `NextPacket` will contain raw payload without the
  // generic payload header.
  RtpPacketizerGeneric(std::span<const uint8_t> payload,
                       PayloadSizeLimits limits);

  ~RtpPacketizerGeneric() override;

  RtpPacketizerGeneric(const RtpPacketizerGeneric&) = delete;
  RtpPacketizerGeneric& operator=(const RtpPacketizerGeneric&) = delete;

  size_t NumPackets() const override;

  // Get the next payload.
  // Write payload and set marker bit of the `packet`.
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* packet) override;

 private:
  // Fills header_ and header_size_ members.
  void BuildHeader(const RTPVideoHeader& rtp_video_header);

  uint8_t header_[3];
  size_t header_size_;
  std::span<const uint8_t> remaining_payload_;
  std::vector<int> payload_sizes_;
  std::vector<int>::const_iterator current_packet_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
