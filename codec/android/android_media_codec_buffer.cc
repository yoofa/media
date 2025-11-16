/*
 * android_media_codec_buffer.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "android_media_codec_buffer.h"

namespace ave {
namespace media {

AndroidMediaCodecBuffer::AndroidMediaCodecBuffer(void* offset, size_t size)
    : CodecBuffer(offset, size) {}

}  // namespace media
}  // namespace ave
