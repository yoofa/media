/*
 * rtp_format_vp9.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP9_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP9_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include <span>
#include "media/foundation/vp9/vp9_globals.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpPacketizerVp9 : public RtpPacketizer {
 public:
  // The `payload` must be one encoded VP9 layer frame.
  RtpPacketizerVp9(std::span<const uint8_t> payload,
                   PayloadSizeLimits limits,
                   const RTPVideoHeaderVP9& hdr);

  ~RtpPacketizerVp9() override;

  RtpPacketizerVp9(const RtpPacketizerVp9&) = delete;
  RtpPacketizerVp9& operator=(const RtpPacketizerVp9&) = delete;

  size_t NumPackets() const override;

  // Gets the next payload with VP9 payload header.
  // Write payload and set marker bit of the `packet`.
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* packet) override;

 private:
  // Writes the payload descriptor header.
  // `layer_begin` and `layer_end` indicates the postision of the packet in
  // the layer frame. Returns false on failure.
  bool WriteHeader(bool layer_begin,
                   bool layer_end,
                   std::span<uint8_t> rtp_payload) const;

  const RTPVideoHeaderVP9 hdr_;
  const int header_size_;
  const int first_packet_extra_header_size_;
  std::span<const uint8_t> remaining_payload_;
  std::vector<int> payload_sizes_;
  std::vector<int>::const_iterator current_packet_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP9_H_
