/*
 * video_codec_type.h - Video codec type enumeration
 *
 * Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-view-engine (AVE) project.
 */

#ifndef MEDIA_FOUNDATION_VIDEO_CODEC_TYPE_H_
#define MEDIA_FOUNDATION_VIDEO_CODEC_TYPE_H_

namespace ave {
namespace media {

enum class VideoCodecType {
  // There are various memset(..., 0, ...) calls in the code that rely on
  // kVideoCodecGeneric being zero.
  kVideoCodecGeneric = 0,
  kVideoCodecVP8,
  kVideoCodecVP9,
  kVideoCodecAV1,
  kVideoCodecH264,
  kVideoCodecH265,
};

}  // namespace media
}  // namespace ave

#endif  // MEDIA_FOUNDATION_VIDEO_CODEC_TYPE_H_
