/*
 * media_packet.h
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_PACKET_H
#define MEDIA_PACKET_H

#include <memory>

#include "buffer.h"
#include "media_meta.h"
#include "media_utils.h"
#include "message_object.h"

namespace ave {
namespace media {

class MediaPacket : MessageObject {
 public:
  enum class PacketBufferType {
    kTypeNormal,
    kTypeNativeHandle,
  };

  static MediaPacket Create(size_t size);
  static MediaPacket CreateWithHandle(void* handle);

  static std::shared_ptr<MediaPacket> CreateShared(size_t size);
  static std::shared_ptr<MediaPacket> CreateSharedWithHandle(void* handle);

  MediaPacket() = delete;
  ~MediaPacket() override;
  // only support copy construct
  MediaPacket(const MediaPacket& other);

  void SetMediaType(MediaType type);

  // will reset size and data
  void SetSize(size_t size);
  void SetData(uint8_t* data, size_t size);

  // get sample info
  // TODO(youfa) complete other MediaType info when merge avelayer
  AudioSampleInfo* audio_info();
  VideoSampleInfo* video_info();

  // use meta() to get media metadata, other functions will be deprecated
  MediaMeta* meta() { return &media_meta_; }

  size_t size() const { return data_ ? data_->size() : size_; }
  std::shared_ptr<Buffer>& buffer() { return data_; }
  const uint8_t* data() const;

  MediaType media_type() const { return media_type_; }
  PacketBufferType buffer_type() const { return buffer_type_; }
  void* native_handle() const { return native_handle_; }

  void SetEOS(bool eos) { is_eos_ = eos; }
  bool is_eos() const { return is_eos_; }

 private:
  MediaPacket(size_t size);
  MediaPacket(void* handle);
  template <typename... Args>
  static std::shared_ptr<MediaPacket> make_shared_internal(Args&&... args) {
    return std::make_shared<MediaPacket>(std::forward<Args>(args)...);
  }

  size_t size_;
  std::shared_ptr<Buffer> data_;
  void* native_handle_;
  PacketBufferType buffer_type_;
  MediaType media_type_;

  bool is_eos_;

  // audio or video or data sample info
  // MediaSampleInfo sample_info_;
  MediaMeta media_meta_;
};

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_PACKET_H */
