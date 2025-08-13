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

  CodecBuffer();
  CodecBuffer(size_t capacity);
  ~CodecBuffer();

  // buffer operation
  uint8_t* base();
  uint8_t* data();
  size_t capacity();
  size_t size();
  size_t offset();
  void SetRange(size_t offset, size_t size);
  void EnsureCapacity(size_t capacity, bool copy = false);
  void ResetBuffer(std::shared_ptr<Buffer>& buffer);

  void SetIndex(int32_t index) {
    buffer_index_ = index;
    buffer_type_ = BufferType::kTypeNormal;
  }
  int32_t index() const { return buffer_index_; }

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
  std::shared_ptr<Buffer> buffer_;
  int32_t buffer_index_;
  int32_t texture_id_;
  void* native_handle_;
  BufferType buffer_type_;
  std::shared_ptr<MediaMeta> format_;
};

}  // namespace media
}  // namespace ave

#endif /* !CODEC_BUFFER_H */
