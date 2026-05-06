/*
 * encoded_image_buffer.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_ENCODED_IMAGE_BUFFER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_ENCODED_IMAGE_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include "base/buffer.h"

#include <memory>

namespace ave {
namespace media {
namespace rtp_rtcp {

// Abstract interface for buffer storage. Intended to support buffers owned by
// external encoders with special release requirements.
class EncodedImageBufferInterface {
 public:
  using value_type = uint8_t;

  virtual const uint8_t* data() const = 0;
  virtual uint8_t* data() = 0;
  virtual size_t size() const = 0;

  const uint8_t* begin() const { return data(); }
  const uint8_t* end() const { return data() + size(); }
};

// Basic implementation of EncodedImageBufferInterface.
class EncodedImageBuffer final : public EncodedImageBufferInterface {
 public:
  static std::shared_ptr<EncodedImageBuffer> Create() { return Create(0); }
  static std::shared_ptr<EncodedImageBuffer> Create(size_t size);
  static std::shared_ptr<EncodedImageBuffer> Create(const uint8_t* data,
                                                    size_t size);
  static std::shared_ptr<EncodedImageBuffer> Create(base::Buffer buffer);

  explicit EncodedImageBuffer(size_t size);
  EncodedImageBuffer(const uint8_t* data, size_t size);
  explicit EncodedImageBuffer(base::Buffer buffer);
  ~EncodedImageBuffer() = default;

  const uint8_t* data() const override;
  uint8_t* data() override;
  size_t size() const override;
  void Realloc(size_t size);

  base::Buffer buffer_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_ENCODED_IMAGE_BUFFER_H_
