/*
 * common_constants.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_FOUNDATION_COMMON_CONSTANTS_H_
#define AVE_MEDIA_FOUNDATION_COMMON_CONSTANTS_H_

#include <cstdint>

namespace ave {
namespace media {

constexpr int16_t kNoPictureId = -1;
constexpr int16_t kNoTl0PicIdx = -1;
constexpr uint8_t kNoTemporalIdx = 0xFF;
constexpr int kNoKeyIdx = -1;

// Note: kMaxSpatialLayers is defined in video_codec_constants.h

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_COMMON_CONSTANTS_H_
