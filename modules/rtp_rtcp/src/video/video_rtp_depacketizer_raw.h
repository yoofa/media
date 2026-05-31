/*
 * video_rtp_depacketizer_raw.h
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_RAW_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_RAW_H_

#include <optional>

#include "base/copy_on_write_buffer.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class VideoRtpDepacketizerRaw : public VideoRtpDepacketizer {
 public:
  ~VideoRtpDepacketizerRaw() override = default;

  std::optional<ParsedRtpPayload> Parse(
      base::CopyOnWriteBuffer rtp_payload) override;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_RAW_H_
