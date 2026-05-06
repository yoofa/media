/*
 * video_rtp_depacketizer_vp9.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_VP9_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_VP9_H_

#include <cstdint>
#include <optional>

#include <span>
#include "base/copy_on_write_buffer.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class VideoRtpDepacketizerVp9 : public VideoRtpDepacketizer {
 public:
  VideoRtpDepacketizerVp9() = default;
  VideoRtpDepacketizerVp9(const VideoRtpDepacketizerVp9&) = delete;
  VideoRtpDepacketizerVp9& operator=(const VideoRtpDepacketizerVp9&) = delete;
  ~VideoRtpDepacketizerVp9() override = default;

  // Parses vp9 rtp payload descriptor.
  // Returns zero on error or vp9 payload header offset on success.
  static int32_t ParseRtpPayload(std::span<const uint8_t> rtp_payload,
                                 RTPVideoHeader* video_header);

  std::optional<ParsedRtpPayload> Parse(
      base::CopyOnWriteBuffer rtp_payload) override;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_VP9_H_
