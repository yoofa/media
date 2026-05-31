/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_

#include <stddef.h>

#include <cstdint>
#include <vector>

#include <span>
#include "media/foundation/vp8/vp8_globals.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Packetizer for VP8.
class RtpPacketizerVp8 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded VP8 frame.
  RtpPacketizerVp8(std::span<const uint8_t> payload,
                   PayloadSizeLimits limits,
                   const RTPVideoHeaderVP8& hdr_info);

  ~RtpPacketizerVp8() override;

  RtpPacketizerVp8(const RtpPacketizerVp8&) = delete;
  RtpPacketizerVp8& operator=(const RtpPacketizerVp8&) = delete;

  size_t NumPackets() const override;

  // Get the next payload with VP8 payload header.
  // Write payload and set marker bit of the `packet`.
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* packet) override;

 private:
  // VP8 header can use up to 6 bytes.
  using RawHeader = absl::InlinedVector<uint8_t, 6>;
  static RawHeader BuildHeader(const RTPVideoHeaderVP8& header);

  RawHeader hdr_;
  std::span<const uint8_t> remaining_payload_;
  std::vector<int> payload_sizes_;
  std::vector<int>::const_iterator current_packet_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_
