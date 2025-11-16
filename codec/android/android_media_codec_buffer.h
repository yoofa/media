/*
 * android_media_codec_buffer.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_ANDROID_MEDIA_CODEC_BUFFER_H_H_
#define AVE_MEDIA_ANDROID_MEDIA_CODEC_BUFFER_H_H_

#include "base/attributes.h"

#include "../codec_buffer.h"
#include "media/foundation/media_errors.h"

namespace ave {

namespace media {

class AndroidMediaCodecBuffer : public CodecBuffer {
 public:
  AndroidMediaCodecBuffer(void* offset, size_t size);
  ~AndroidMediaCodecBuffer() override;

  status_t SetRange(size_t offset AVE_MAYBE_UNUSED,
                    size_t size AVE_MAYBE_UNUSED) override {
    return ERROR_UNSUPPORTED;
  }

  status_t EnsureCapacity(size_t capacity AVE_MAYBE_UNUSED,
                          bool copy AVE_MAYBE_UNUSED) override {
    return ERROR_UNSUPPORTED;
  }

  status_t ResetBuffer(
      std::shared_ptr<media::Buffer>& buffer AVE_MAYBE_UNUSED) override {
    return ERROR_UNSUPPORTED;
  }
};

}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_ANDROID_MEDIA_CODEC_BUFFER_H_H_ */
