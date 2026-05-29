/*
 * video_rotation.h - Video rotation enumeration
 *
 * Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-view-engine (AVE) project.
 */

#ifndef MEDIA_FOUNDATION_VIDEO_ROTATION_H_
#define MEDIA_FOUNDATION_VIDEO_ROTATION_H_

namespace ave {
namespace media {

// enum for clockwise rotation.
// Not using enum class for backward compatibility with existing code.
enum VideoRotation {
  kVideoRotation_0 = 0,
  kVideoRotation_90 = 90,
  kVideoRotation_180 = 180,
  kVideoRotation_270 = 270
};

}  // namespace media
}  // namespace ave

#endif  // MEDIA_FOUNDATION_VIDEO_ROTATION_H_
