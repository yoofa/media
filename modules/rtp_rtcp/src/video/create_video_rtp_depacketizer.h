/*
 * create_video_rtp_depacketizer.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_CREATE_VIDEO_RTP_DEPACKETIZER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_CREATE_VIDEO_RTP_DEPACKETIZER_H_

#include <memory>

#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

std::unique_ptr<VideoRtpDepacketizer> CreateVideoRtpDepacketizer(
    VideoCodecType codec);

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_CREATE_VIDEO_RTP_DEPACKETIZER_H_
