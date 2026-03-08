/*
 * android_ndk_media_codec.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_ANDROID_NDK_MEDIA_CODEC_H_H_
#define AVE_MEDIA_ANDROID_NDK_MEDIA_CODEC_H_H_

#include <mutex>
#include <unordered_map>

#include "../codec.h"

struct AMediaCodec;
struct AMediaFormat;

namespace ave {
namespace media {

/**
 * Android NDK MediaCodec wrapper implementing the Codec interface.
 * Uses the AMediaCodec C API (NdkMediaCodec.h) for hardware-accelerated
 * encoding/decoding on Android.
 */
class AndroidNdkMediaCodec : public Codec {
 public:
  enum CreatedBy {
    kCreatedByName,
    kCreatedByMime,
  };
  explicit AndroidNdkMediaCodec(const char* mime_or_name,
                                CreatedBy type = kCreatedByName,
                                bool encoder = true);
  ~AndroidNdkMediaCodec() override;

  status_t Configure(const std::shared_ptr<CodecConfig>& config) override;
  status_t SetCallback(CodecCallback* callback) override;
  status_t Start() override;
  status_t Stop() override;
  status_t Flush() override;
  status_t Reset() override;
  status_t Release() override;

  status_t GetInputBuffer(size_t index,
                          std::shared_ptr<CodecBuffer>& buffer) override;
  status_t GetOutputBuffer(size_t index,
                           std::shared_ptr<CodecBuffer>& buffer) override;
  ssize_t DequeueInputBuffer(int64_t timeoutUs) override;
  status_t QueueInputBuffer(size_t index) override;
  ssize_t DequeueOutputBuffer(int64_t timeout_us) override;
  status_t ReleaseOutputBuffer(size_t index, bool render) override;

  void NotifyInputBufferAvailable(size_t index);
  void NotifyOutputBufferAvailable(size_t index,
                                   int32_t offset,
                                   int32_t size,
                                   int64_t presentation_time_us,
                                   uint32_t flags);
  void NotifyOutputFormatChanged(AMediaFormat* format);
  void NotifyError(status_t error);
  void NotifyFrameRendered(std::shared_ptr<Message> message);

 private:
  /**
   * Converts MediaMeta track format to AMediaFormat for codec configuration.
   * @param format Source media metadata.
   * @return AMediaFormat pointer (caller must delete with AMediaFormat_delete).
   */
  AMediaFormat* MediaMetaToAMediaFormat(
      const std::shared_ptr<MediaMeta>& format);

  struct OutputBufferInfo {
    int32_t offset = 0;
    int32_t size = 0;
    int64_t presentation_time_us = 0;
    uint32_t flags = 0;
  };

  std::mutex mutex_;
  AMediaCodec* android_media_codec_ = nullptr;
  CodecCallback* callback_ = nullptr;
  bool is_encoder_ = false;
  std::string mime_;

  // <index, CodecBuffer>
  std::unordered_map<size_t, std::shared_ptr<CodecBuffer>> input_buffers_;
  std::unordered_map<size_t, std::shared_ptr<CodecBuffer>> output_buffers_;
  // Store output buffer info from dequeue/callback for size/offset/pts
  std::unordered_map<size_t, OutputBufferInfo> output_buffer_infos_;
};

}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_ANDROID_NDK_MEDIA_CODEC_H_H_ */
