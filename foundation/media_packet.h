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
#include "media_format.h"
#include "media_utils.h"
#include "message_object.h"

namespace ave {
namespace media {

class MediaPacket : MessageObject {
 protected:
  // for private construct
  struct protect_parameter {
    explicit protect_parameter() = default;
  };

 public:
  enum class PacketBufferType {
    kTypeNormal,
    kTypeNativeHandle,
  };

  static MediaPacket Create(size_t size);
  static MediaPacket CreateWithHandle(void* handle);

 private:
  explicit MediaPacket(size_t size, protect_parameter);
  explicit MediaPacket(void* handle, protect_parameter);

 public:
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

  size_t size() const { return size_; }
  std::shared_ptr<Buffer>& buffer() { return data_; }
  const uint8_t* data() const;

  MediaType media_type() const { return media_type_; }
  PacketBufferType buffer_type() const { return buffer_type_; }
  void* native_handle() const { return native_handle_; }

  void SetEOS(bool eos) { is_eos_ = eos; }
  bool is_eos() const { return is_eos_; }

 private:
  size_t size_;
  std::shared_ptr<Buffer> data_;
  void* native_handle_;
  PacketBufferType buffer_type_;
  MediaType media_type_;

  bool is_eos_;

  // audio or video or data sample info
  // MediaSampleInfo sample_info_;
  MediaFormat media_format_;
};

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_PACKET_H */
