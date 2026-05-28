/*
 * av1_globals.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_FOUNDATION_AV1_AV1_GLOBALS_H_
#define AVE_MEDIA_FOUNDATION_AV1_AV1_GLOBALS_H_

#include <stdint.h>

namespace ave {
namespace media {

// AV1 codec specific structures for RTP
// Based on AV1 RTP payload format specification

struct RTPVideoHeaderAV1 {
  // Currently AV1 doesn't have much specific header data for RTP
  // The codec type is identified through the generic video header
};

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_AV1_AV1_GLOBALS_H_
