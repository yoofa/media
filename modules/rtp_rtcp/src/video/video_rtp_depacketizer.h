/*
 * video_rtp_depacketizer.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H_

#include <stdint.h>

#include <optional>

#include <memory>
#include <span>
#include "base/copy_on_write_buffer.h"

#include "media/modules/rtp_rtcp/api/encoded_image_buffer.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class VideoRtpDepacketizer {
 public:
  struct ParsedRtpPayload {
    RTPVideoHeader video_header;
    base::CopyOnWriteBuffer video_payload;
  };

  virtual ~VideoRtpDepacketizer() = default;
  virtual std::optional<ParsedRtpPayload> Parse(
      base::CopyOnWriteBuffer rtp_payload) = 0;

  // Assembles an EncodedFrame from a collection of RTP payloads.
  // Used for codecs that require frame assembly from multiple packets (e.g.
  // AV1). Default implementation returns nullptr.
  virtual std::shared_ptr<EncodedImageBuffer> AssembleFrame(
      std::span<const std::span<const uint8_t>> rtp_payloads);
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H_
