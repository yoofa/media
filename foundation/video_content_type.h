/*
 * video_content_type.h - Video content type enumeration
 *
 * Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-view-engine (AVE) project.
 */

#ifndef MEDIA_FOUNDATION_VIDEO_CONTENT_TYPE_H_
#define MEDIA_FOUNDATION_VIDEO_CONTENT_TYPE_H_

#include <cstdint>

namespace ave {
namespace media {

// VideoContentType stored as a single byte, which is sent over the network
// in the rtp-hdrext/video-content-type extension.
// Only the lowest bit is used, per the enum.
enum class VideoContentType : uint8_t {
  UNSPECIFIED = 0,
  SCREENSHARE = 1,
};

namespace videocontenttypehelpers {

inline bool IsScreenshare(const VideoContentType& content_type) {
  return content_type == VideoContentType::SCREENSHARE;
}

inline bool IsValidContentType(uint8_t value) {
  return value <= 1;
}

inline const char* ToString(const VideoContentType& content_type) {
  switch (content_type) {
    case VideoContentType::UNSPECIFIED:
      return "unspecified";
    case VideoContentType::SCREENSHARE:
      return "screenshare";
  }
  return "unknown";
}

}  // namespace videocontenttypehelpers

}  // namespace media
}  // namespace ave

#endif  // MEDIA_FOUNDATION_VIDEO_CONTENT_TYPE_H_
