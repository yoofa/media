/*
 * video_rtp_depacketizer_h264.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_

#include <optional>

#include "base/copy_on_write_buffer.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class VideoRtpDepacketizerH264 : public VideoRtpDepacketizer {
 public:
  ~VideoRtpDepacketizerH264() override = default;

  std::optional<ParsedRtpPayload> Parse(
      base::CopyOnWriteBuffer rtp_payload) override;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_
