/*
 * media_frame.h
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef media_frame_H
#define media_frame_H

#include <memory>

#include "buffer.h"
#include "media_meta.h"

namespace ave {
namespace media {

class MediaFrame : public MediaMeta {
 public:
  enum class FrameBufferType {
    kTypeNormal,
    kTypeNativeHandle,
  };

  static MediaFrame Create(size_t size = 0,
                           MediaType media_type = MediaType::AUDIO);

  static std::shared_ptr<MediaFrame> CreateShared(
      size_t size = 0,
      MediaType media_type = MediaType::AUDIO);

  MediaFrame() = delete;
  MediaFrame(size_t size, MediaType media_type);
  ~MediaFrame() override = default;
  // only support copy construct
  MediaFrame(const MediaFrame& other);

  // use meta() to get media metadata, other functions will be deprecated
  MediaMeta* meta() { return this; }

  // buffer releated
  void SetSize(size_t size);
  void SetData(uint8_t* data, size_t size);
  size_t size() const { return data_ ? data_->size() : 0; }
  std::shared_ptr<media::Buffer>& buffer() { return data_; }
  const uint8_t* data() const;

  // releated with this class
  // TODO(youfa): other sample info
  AudioSampleInfo* audio_info();
  VideoSampleInfo* video_info();

  void SetNativeHandle(void* handle);
  FrameBufferType buffer_type() const { return buffer_type_; }
  void* native_handle() const { return native_handle_; }

 private:
  std::shared_ptr<media::Buffer> data_;
  FrameBufferType buffer_type_;
  void* native_handle_;
};

}  // namespace media
}  // namespace ave

#endif /* !media_frame_H */
