/*
 * video_codec_constants.h - Video codec constants
 *
 * Copyright (c) 2025 The AveStart project authors. All Rights Reserved.
 *
 * Ported from WebRTC project.
 */

#ifndef MEDIA_FOUNDATION_VIDEO_CODEC_CONSTANTS_H_
#define MEDIA_FOUNDATION_VIDEO_CODEC_CONSTANTS_H_

namespace ave {
namespace media {

enum : int { kMaxEncoderBuffers = 8 };
enum : int { kMaxSimulcastStreams = 3 };
enum : int { kMaxSpatialLayers = 5 };
enum : int { kMaxTemporalStreams = 4 };
enum : int { kMaxPreferredPixelFormats = 5 };

}  // namespace media
}  // namespace ave

#endif  // MEDIA_FOUNDATION_VIDEO_CODEC_CONSTANTS_H_
