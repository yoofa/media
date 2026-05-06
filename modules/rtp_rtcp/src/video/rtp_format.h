/*
 * rtp_format.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include <span>
#include "media/foundation/video_codec_type.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpPacketToSend;
struct RTPVideoHeader;

class RtpPacketizer {
 public:
  struct PayloadSizeLimits {
    int32_t max_payload_len = 1200;
    int32_t first_packet_reduction_len = 0;
    int32_t last_packet_reduction_len = 0;
    // Reduction len for packet that is first & last at the same time.
    int32_t single_packet_reduction_len = 0;
  };

  virtual ~RtpPacketizer() = default;

  // Returns number of remaining packets to produce by the packetizer.
  virtual size_t NumPackets() const = 0;

  // Get the next payload with payload header.
  // Write payload and set marker bit of the `packet`.
  // Returns true on success, false otherwise.
  virtual bool NextPacket(RtpPacketToSend* packet) = 0;

  // Split payload_len into sum of integers with respect to `limits`.
  // Returns empty vector on failure.
  static std::vector<int32_t> SplitAboutEqually(
      int32_t payload_len,
      const PayloadSizeLimits& limits);

  // Factory method to create codec-specific packetizer.
  // Returns nullptr if codec type is not supported.
  // Note: Implementation is in rtp_format.cc which requires linking
  // with all packetizer implementations.
  static std::unique_ptr<RtpPacketizer> Create(
      std::optional<VideoCodecType> type,
      std::span<const uint8_t> payload,
      PayloadSizeLimits limits,
      const RTPVideoHeader& rtp_video_header,
      bool enable_av1_even_split = true);
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
