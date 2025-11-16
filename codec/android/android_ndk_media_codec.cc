/*
 * android_ndk_media_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "android_ndk_media_codec.h"
#include <media/NdkMediaError.h>

#include "base/attributes.h"

// android
#include "base/errors.h"
#include "media/NdkMediaCodec.h"
#include "media/foundation/media_errors.h"

namespace ave {
namespace media {

namespace {
class AMediaCodecCallbackHelper : public AMediaCodecOnAsyncNotifyCallback {
 public:
  static void onAsyncInputAvailable(AMediaCodec* codec AVE_MAYBE_UNUSED,
                                    void* userdata,
                                    int32_t index) {
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    c->NotifyInputBufferAvailable(index);
  }

  static void onAsyncOutputAvailable(AMediaCodec* codec AVE_MAYBE_UNUSED,
                                     void* userdata,
                                     int32_t index,
                                     AMediaCodecBufferInfo* bufferInfo
                                         AVE_MAYBE_UNUSED) {
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    c->NotifyOutputBufferAvailable(index);
  }

  static void onAsyncFormatChanged(AMediaCodec* codec AVE_MAYBE_UNUSED,
                                   void* userdata,
                                   AMediaFormat* format AVE_MAYBE_UNUSED) {
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);

    // TODO
    auto meta = MediaMeta::CreatePtr();
    c->NotifyOutputFormatChanged(meta);
  }

  static void onAsyncError(AMediaCodec* codec AVE_MAYBE_UNUSED,
                           void* userdata,
                           media_status_t error,
                           int32_t actionCode AVE_MAYBE_UNUSED,
                           const char* detail AVE_MAYBE_UNUSED) {
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    c->NotifyError(static_cast<status_t>(error));
  }

  static void onFrameRendered(AMediaCodec* codec AVE_MAYBE_UNUSED,
                              void* userdata,
                              int64_t mediaTimeUs AVE_MAYBE_UNUSED,
                              int64_t systemNano AVE_MAYBE_UNUSED) {
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    auto msg = std::make_shared<Message>();
    c->NotifyFrameRendered(msg);
  }
};
}  // namespace

AndroidNdkMediaCodec::AndroidNdkMediaCodec(const char* mime_or_name,
                                           CreatedBy type,
                                           bool encoder) {
  if (type == kCreatedByName) {
    android_media_codec_ = AMediaCodec_createCodecByName(mime_or_name);
  } else if (encoder) {
    android_media_codec_ = AMediaCodec_createEncoderByType(mime_or_name);
  } else {
    android_media_codec_ = AMediaCodec_createDecoderByType(mime_or_name);
  }
}

AndroidNdkMediaCodec::~AndroidNdkMediaCodec() {
  if (android_media_codec_) {
    AMediaCodec_delete(android_media_codec_);
    android_media_codec_ = nullptr;
  }
}

status_t AndroidNdkMediaCodec::Configure(
    const std::shared_ptr<CodecConfig>& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO
  return OK;
}

status_t AndroidNdkMediaCodec::SetCallback(CodecCallback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callback_ = callback;
  AMediaCodec_setAsyncNotifyCallback(android_media_codec_,
                                     AMediaCodecCallbackHelper{}, this);
  AMediaCodec_setOnFrameRenderedCallback(
      android_media_codec_, AMediaCodecCallbackHelper::onFrameRendered, this);
  return OK;
}

status_t AndroidNdkMediaCodec::Start() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto ret = AMediaCodec_start(android_media_codec_);
  // FIXME: ret conversion
  return static_cast<status_t>(ret);
}

status_t AndroidNdkMediaCodec::Stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto ret = AMediaCodec_stop(android_media_codec_);
  return static_cast<status_t>(ret);
}

status_t AndroidNdkMediaCodec::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto ret = AMediaCodec_flush(android_media_codec_);
  return static_cast<status_t>(ret);
}

status_t AndroidNdkMediaCodec::Reset() {
  // TODO
  return OK;
}

status_t AndroidNdkMediaCodec::Release() {
  // TODO
  return OK;
}

status_t AndroidNdkMediaCodec::GetInputBuffer(
    size_t index,
    std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t size{};
  auto* addr = AMediaCodec_getInputBuffer(android_media_codec_, index, &size);
  if (!addr) {
    return ERROR_RETRY;
  }
  if (input_buffers_.find(index) == input_buffers_.end()) {
    auto codec_buffer = std::make_shared<CodecBuffer>(addr, size);
    input_buffers_[index] = codec_buffer;
  } else {
    auto& codec_buffer = input_buffers_[index];
    auto new_buffer = std::make_shared<media::Buffer>(addr, size);
    codec_buffer->ResetBuffer(new_buffer);
  }
  buffer = input_buffers_[index];
  return OK;
}

status_t AndroidNdkMediaCodec::GetOutputBuffer(
    size_t index,
    std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t size{};
  auto* addr = AMediaCodec_getOutputBuffer(android_media_codec_, index, &size);
  if (!addr) {
    return UNKNOWN_ERROR;
  }

  if (output_buffers_.find(index) == output_buffers_.end()) {
    auto codec_buffer = std::make_shared<CodecBuffer>(addr, size);
    output_buffers_[index] = codec_buffer;
  } else {
    auto& codec_buffer = output_buffers_[index];
    auto new_buffer = std::make_shared<media::Buffer>(addr, size);
    codec_buffer->ResetBuffer(new_buffer);
  }
  buffer = output_buffers_[index];
  return OK;
}

ssize_t AndroidNdkMediaCodec::DequeueInputBuffer(int64_t timeoutUs) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto index = AMediaCodec_dequeueInputBuffer(android_media_codec_, timeoutUs);
  return static_cast<ssize_t>(index);
}

status_t AndroidNdkMediaCodec::QueueInputBuffer(size_t index) {
  if (input_buffers_.find(index) == input_buffers_.end()) {
    return INVALID_OPERATION;
  }
  auto& input_buffer = input_buffers_[index];
  auto offset = input_buffer->offset();
  auto size = input_buffer->size();
  auto pts = input_buffer->format()->pts();
  // TODO: flags

  auto ret = AMediaCodec_queueInputBuffer(android_media_codec_, index,
                                          static_cast<off_t>(offset), size,
                                          pts.us_or(0), 0);

  return static_cast<status_t>(ret);
}

status_t AndroidNdkMediaCodec::DequeueOutputBuffer(int64_t timeout_us) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto index = AMediaCodec_dequeueOutputBuffer(android_media_codec_, nullptr,
                                               timeout_us);
  return static_cast<status_t>(index);
}

status_t AndroidNdkMediaCodec::ReleaseOutputBuffer(size_t index, bool render) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (output_buffers_.find(index) == output_buffers_.end()) {
    return INVALID_OPERATION;
  }
  auto ret =
      AMediaCodec_releaseOutputBuffer(android_media_codec_, index, render);
  return static_cast<status_t>(ret);
}

void AndroidNdkMediaCodec::NotifyInputBufferAvailable(size_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (callback_) {
    callback_->OnInputBufferAvailable(index);
  }
}

void AndroidNdkMediaCodec::NotifyOutputBufferAvailable(size_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (callback_) {
    callback_->OnOutputBufferAvailable(index);
  }
}

void AndroidNdkMediaCodec::NotifyOutputFormatChanged(
    const std::shared_ptr<MediaMeta>& format) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (callback_) {
    callback_->OnOutputFormatChanged(format);
  }
}

void AndroidNdkMediaCodec::NotifyError(status_t error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (callback_) {
    callback_->OnError(error);
  }
}

void AndroidNdkMediaCodec::NotifyFrameRendered(
    std::shared_ptr<Message> message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (callback_) {
    callback_->OnFrameRendered(message);
  }
}

}  // namespace media
}  // namespace ave
