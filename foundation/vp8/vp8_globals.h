/*
 * vp8_globals.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_FOUNDATION_VP8_VP8_GLOBALS_H_
#define AVE_MEDIA_FOUNDATION_VP8_VP8_GLOBALS_H_

#include "media/foundation/common_constants.h"

namespace ave {
namespace media {

struct RTPVideoHeaderVP8 {
  void InitRTPVideoHeaderVP8() {
    nonReference = false;
    pictureId = kNoPictureId;
    tl0PicIdx = kNoTl0PicIdx;
    temporalIdx = kNoTemporalIdx;
    layerSync = false;
    keyIdx = kNoKeyIdx;
    partitionId = 0;
    beginningOfPartition = false;
  }

  friend bool operator==(const RTPVideoHeaderVP8& lhs,
                         const RTPVideoHeaderVP8& rhs) {
    return lhs.nonReference == rhs.nonReference &&
           lhs.pictureId == rhs.pictureId && lhs.tl0PicIdx == rhs.tl0PicIdx &&
           lhs.temporalIdx == rhs.temporalIdx &&
           lhs.layerSync == rhs.layerSync && lhs.keyIdx == rhs.keyIdx &&
           lhs.partitionId == rhs.partitionId &&
           lhs.beginningOfPartition == rhs.beginningOfPartition;
  }

  friend bool operator!=(const RTPVideoHeaderVP8& lhs,
                         const RTPVideoHeaderVP8& rhs) {
    return !(lhs == rhs);
  }

  bool nonReference;          // Frame is discardable.
  int16_t pictureId;          // Picture ID index, 15 bits;
                              // kNoPictureId if PictureID does not exist.
  int16_t tl0PicIdx;          // TL0PIC_IDX, 8 bits;
                              // kNoTl0PicIdx means no value provided.
  uint8_t temporalIdx;        // Temporal layer index, or kNoTemporalIdx.
  bool layerSync;             // This frame is a layer sync frame.
                              // Disabled if temporalIdx == kNoTemporalIdx.
  int32_t keyIdx;             // 5 bits; kNoKeyIdx means not used.
  int32_t partitionId;        // VP8 partition ID
  bool beginningOfPartition;  // True if this packet is the first
                              // in a VP8 partition. Otherwise false
};

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_VP8_VP8_GLOBALS_H_
