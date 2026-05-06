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

enum : int32_t { kMaxEncoderBuffers = 8 };
enum : int32_t { kMaxSimulcastStreams = 3 };
enum : int32_t { kMaxSpatialLayers = 5 };
enum : int32_t { kMaxTemporalStreams = 4 };
enum : int32_t { kMaxPreferredPixelFormats = 5 };

}  // namespace media
}  // namespace ave

#endif  // MEDIA_FOUNDATION_VIDEO_CODEC_CONSTANTS_H_
