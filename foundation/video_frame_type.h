/*
 * video_frame_type.h - Video frame type enumeration
 *
 * Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-view-engine (AVE) project.
 */

#ifndef MEDIA_FOUNDATION_VIDEO_FRAME_TYPE_H_
#define MEDIA_FOUNDATION_VIDEO_FRAME_TYPE_H_

#include <string_view>

namespace ave {
namespace media {

enum class VideoFrameType {
  kEmptyFrame = 0,
  // Wire format for MultiplexEncodedImagePacker seems to depend on numerical
  // values of these constants.
  kVideoFrameKey = 3,
  kVideoFrameDelta = 4,
};

inline constexpr std::string_view VideoFrameTypeToString(
    VideoFrameType frame_type) {
  switch (frame_type) {
    case VideoFrameType::kEmptyFrame:
      return "empty";
    case VideoFrameType::kVideoFrameKey:
      return "key";
    case VideoFrameType::kVideoFrameDelta:
      return "delta";
  }
  return "";
}

}  // namespace media
}  // namespace ave

#endif  // MEDIA_FOUNDATION_VIDEO_FRAME_TYPE_H_
