/*
 * media_frame.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_FRAME_H
#define MEDIA_FRAME_H

#include <memory>

#include "buffer.h"
#include "media_utils.h"

namespace ave {
namespace media {

struct AudioFrameInfo {
  int32_t sample_rate;
  int32_t channels;
  int32_t bits_per_sample;
  int64_t timestamp_us;
};

struct VideoFrameInfo {
  int32_t width;
  int32_t height;
  int32_t stride;
  PixelFormat pixel_format;
  int64_t timestamp_us;
};

class MediaFrame {
 protected:
  // for private construct
  struct protect_parameter {
    explicit protect_parameter() = default;
  };

 public:
  enum class FrameBufferType {
    kTypeNormal,       // Normal buffer data
    kTypeNativeHandle  // Native handle (e.g., hardware buffer)
  };

  static MediaFrame Create(size_t size);
  static MediaFrame CreateWithHandle(void* handle);

  MediaFrame(const MediaFrame& other);
  ~MediaFrame();

  // Media type operations
  void SetMediaType(MediaType type);
  MediaType GetMediaType() const { return media_type_; }

  // Buffer operations
  void SetSize(size_t size);
  void SetData(uint8_t* data, size_t size);
  const uint8_t* data() const;
  size_t size() const { return size_; }

  // Frame info access
  AudioSampleInfo* audio_info();
  VideoSampleInfo* video_info();

  // Buffer type
  FrameBufferType buffer_type() const { return buffer_type_; }
  void* native_handle() const { return native_handle_; }

 private:
  MediaFrame(size_t size, protect_parameter);
  MediaFrame(void* handle, protect_parameter);

  size_t size_;
  std::shared_ptr<Buffer> data_;

  void* native_handle_;
  FrameBufferType buffer_type_;

  MediaType media_type_;
  // audio or video frame info
  MediaSampleInfo sample_info_;
};

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_FRAME_H */
