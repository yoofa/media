/*
 * video_rtp_depacketizer_av1.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_AV1_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_AV1_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>

#include <memory>
#include <span>
#include "base/copy_on_write_buffer.h"

#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class VideoRtpDepacketizerAv1 : public VideoRtpDepacketizer {
 public:
  VideoRtpDepacketizerAv1() = default;
  VideoRtpDepacketizerAv1(const VideoRtpDepacketizerAv1&) = delete;
  VideoRtpDepacketizerAv1& operator=(const VideoRtpDepacketizerAv1&) = delete;
  ~VideoRtpDepacketizerAv1() override = default;

  std::shared_ptr<EncodedImageBuffer> AssembleFrame(
      std::span<const std::span<const uint8_t>> rtp_payloads) override;

  std::optional<ParsedRtpPayload> Parse(
      base::CopyOnWriteBuffer rtp_payload) override;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_AV1_H_
