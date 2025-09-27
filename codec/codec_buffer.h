/*
 * codec_buffer.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef CODEC_BUFFER_H
#define CODEC_BUFFER_H

#include "../foundation/buffer.h"
#include "../foundation/media_meta.h"

namespace ave {
namespace media {

class CodecBuffer {
 public:
  enum class BufferType {
    kTypeNormal,        // a normal memory buffer
    kTypeTexture,       // frame is rendered in texture_id
    kTypeNativeHandle,  // a buffer with specified native handle that only some
                        // special sink can resolve real data
  };

  CodecBuffer(void* data, size_t size);
  CodecBuffer(size_t capacity);
  virtual ~CodecBuffer();

  // buffer operation
  virtual uint8_t* base();
  virtual uint8_t* data();
  virtual size_t capacity();
  virtual size_t size();
  virtual size_t offset();

  virtual status_t SetRange(size_t offset, size_t size);
  virtual status_t EnsureCapacity(size_t capacity, bool copy);

  virtual status_t ResetBuffer(std::shared_ptr<media::Buffer>& buffer);

  void SetTextureId(int32_t texture_id) {
    texture_id_ = texture_id;
    buffer_type_ = BufferType::kTypeTexture;
  }
  int32_t texture_id() const { return texture_id_; }

  void SetNativeHandle(void* handle) {
    native_handle_ = handle;
    buffer_type_ = BufferType::kTypeNativeHandle;
  }
  void* native_handle() const { return native_handle_; }

  BufferType buffer_type() const { return buffer_type_; }

  std::shared_ptr<MediaMeta>& format() { return format_; }

 private:
  std::shared_ptr<media::Buffer> buffer_;
  int32_t texture_id_;
  void* native_handle_;
  BufferType buffer_type_;
  std::shared_ptr<MediaMeta> format_;
};

}  // namespace media
}  // namespace ave

#endif /* !CODEC_BUFFER_H */
