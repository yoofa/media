/*
 * android_ndk_media_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "android_ndk_media_codec.h"

#include <media/NdkMediaError.h>
#include <media/NdkMediaFormat.h>

#include "base/attributes.h"
#include "base/errors.h"
#include "base/logging.h"
#include "media/NdkMediaCodec.h"
#include "media/audio/channel_layout.h"
#include "media/foundation/media_errors.h"

namespace ave {
namespace media {

namespace {

// Map media_status_t to ave status_t
status_t MapMediaStatus(media_status_t status) {
  switch (status) {
    case AMEDIA_OK:
      return OK;
    case AMEDIA_ERROR_UNKNOWN:
      return UNKNOWN_ERROR;
    case AMEDIA_ERROR_MALFORMED:
      return ERROR_MALFORMED;
    case AMEDIA_ERROR_UNSUPPORTED:
      return ERROR_UNSUPPORTED;
    case AMEDIA_ERROR_INVALID_OBJECT:
      return INVALID_OPERATION;
    case AMEDIA_ERROR_INVALID_PARAMETER:
      return BAD_VALUE;
    default:
      return UNKNOWN_ERROR;
  }
}

class AMediaCodecCallbackHelper : public AMediaCodecOnAsyncNotifyCallback {
 public:
  static void onAsyncInputAvailable(AMediaCodec* codec AVE_MAYBE_UNUSED,
                                    void* userdata,
                                    int32_t index) {
    AVE_LOG(LS_VERBOSE) << "NDK callback: input buffer available, index="
                        << index;
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    c->NotifyInputBufferAvailable(static_cast<size_t>(index));
  }

  static void onAsyncOutputAvailable(AMediaCodec* codec AVE_MAYBE_UNUSED,
                                     void* userdata,
                                     int32_t index,
                                     AMediaCodecBufferInfo* bufferInfo) {
    AVE_LOG(LS_VERBOSE) << "NDK callback: output buffer available, index="
                        << index
                        << ", size=" << (bufferInfo ? bufferInfo->size : -1)
                        << ", pts="
                        << (bufferInfo ? bufferInfo->presentationTimeUs : -1)
                        << ", flags=" << (bufferInfo ? bufferInfo->flags : -1);
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    c->NotifyOutputBufferAvailable(
        static_cast<size_t>(index), bufferInfo ? bufferInfo->offset : 0,
        bufferInfo ? bufferInfo->size : 0,
        bufferInfo ? bufferInfo->presentationTimeUs : 0,
        bufferInfo ? static_cast<uint32_t>(bufferInfo->flags) : 0);
  }

  static void onAsyncFormatChanged(AMediaCodec* codec AVE_MAYBE_UNUSED,
                                   void* userdata,
                                   AMediaFormat* format) {
    AVE_LOG(LS_INFO) << "NDK callback: format changed: "
                     << (format ? AMediaFormat_toString(format) : "null");
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    c->NotifyOutputFormatChanged(format);
  }

  static void onAsyncError(AMediaCodec* codec AVE_MAYBE_UNUSED,
                           void* userdata,
                           media_status_t error,
                           int32_t actionCode AVE_MAYBE_UNUSED,
                           const char* detail) {
    AVE_LOG(LS_ERROR) << "NDK callback: codec error=" << error
                      << ", actionCode=" << actionCode
                      << ", detail=" << (detail ? detail : "none");
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    c->NotifyError(MapMediaStatus(error));
  }

  static void onFrameRendered(AMediaCodec* codec AVE_MAYBE_UNUSED,
                              void* userdata,
                              int64_t mediaTimeUs,
                              int64_t systemNano AVE_MAYBE_UNUSED) {
    AVE_LOG(LS_VERBOSE) << "NDK callback: frame rendered, mediaTimeUs="
                        << mediaTimeUs;
    auto* c = reinterpret_cast<AndroidNdkMediaCodec*>(userdata);
    auto msg = std::make_shared<Message>();
    c->NotifyFrameRendered(msg);
  }
};
}  // namespace

AndroidNdkMediaCodec::AndroidNdkMediaCodec(const char* mime_or_name,
                                           CreatedBy type,
                                           bool encoder)
    : is_encoder_(encoder) {
  AVE_LOG(LS_INFO) << "AndroidNdkMediaCodec: creating codec, mime_or_name="
                   << mime_or_name << ", type="
                   << (type == kCreatedByName ? "byName" : "byMime")
                   << ", encoder=" << encoder;

  if (type == kCreatedByName) {
    android_media_codec_ = AMediaCodec_createCodecByName(mime_or_name);
  } else if (encoder) {
    android_media_codec_ = AMediaCodec_createEncoderByType(mime_or_name);
  } else {
    android_media_codec_ = AMediaCodec_createDecoderByType(mime_or_name);
  }

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "AndroidNdkMediaCodec: failed to create codec for "
                      << mime_or_name;
  } else {
    AVE_LOG(LS_INFO) << "AndroidNdkMediaCodec: codec created successfully";
  }
}

AndroidNdkMediaCodec::~AndroidNdkMediaCodec() {
  AVE_LOG(LS_VERBOSE) << "AndroidNdkMediaCodec: destructor";
  if (android_media_codec_) {
    AMediaCodec_delete(android_media_codec_);
    android_media_codec_ = nullptr;
  }
}

AMediaFormat* AndroidNdkMediaCodec::MediaMetaToAMediaFormat(
    const std::shared_ptr<MediaMeta>& format) {
  AMediaFormat* ndk_format = AMediaFormat_new();
  if (!ndk_format) {
    AVE_LOG(LS_ERROR) << "MediaMetaToAMediaFormat: AMediaFormat_new failed";
    return nullptr;
  }

  const std::string& mime = format->mime();
  AMediaFormat_setString(ndk_format, AMEDIAFORMAT_KEY_MIME, mime.c_str());
  AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: mime=" << mime;

  if (format->stream_type() == MediaType::VIDEO) {
    int32_t width = format->width();
    int32_t height = format->height();
    if (width > 0 && height > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_WIDTH, width);
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_HEIGHT, height);
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: video " << width << "x"
                          << height;
    }

    int32_t fps = format->fps();
    if (fps > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: fps=" << fps;
    }

    int32_t profile = format->codec_profile();
    if (profile > 0) {
      // AMEDIAFORMAT_KEY_PROFILE requires API 28+
      if (__builtin_available(android 28, *)) {
        AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_PROFILE, profile);
      }
    }

    int32_t level = format->codec_level();
    if (level > 0) {
      if (__builtin_available(android 28, *)) {
        AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_LEVEL, level);
      }
    }

    int16_t rotation = format->rotation();
    if (rotation != 0) {
      if (__builtin_available(android 28, *)) {
        AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_ROTATION, rotation);
      }
    }

  } else if (format->stream_type() == MediaType::AUDIO) {
    uint32_t sample_rate = format->sample_rate();
    if (sample_rate > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                            static_cast<int32_t>(sample_rate));
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: sample_rate="
                          << sample_rate;
    }

    ChannelLayout layout = format->channel_layout();
    int channel_count = ChannelLayoutToChannelCount(layout);
    if (channel_count > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                            channel_count);
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: channels="
                          << channel_count;
    }

    int64_t bitrate = format->bitrate();
    if (bitrate > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_BIT_RATE,
                            static_cast<int32_t>(bitrate));
    }
  }

  // CSD (Codec Specific Data) - SPS/PPS for H.264, AudioSpecificConfig, etc.
  auto csd = format->private_data();
  if (csd && csd->size() > 0) {
    AMediaFormat_setBuffer(ndk_format, "csd-0", csd->data(), csd->size());
    AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: csd-0 size="
                        << csd->size();
  }

  AVE_LOG(LS_INFO) << "MediaMetaToAMediaFormat: result="
                   << AMediaFormat_toString(ndk_format);
  return ndk_format;
}

status_t AndroidNdkMediaCodec::Configure(
    const std::shared_ptr<CodecConfig>& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "Configure: codec not created";
    return NO_INIT;
  }

  if (!config || !config->format) {
    AVE_LOG(LS_ERROR) << "Configure: invalid config or format";
    return BAD_VALUE;
  }

  mime_ = config->info.mime;
  AVE_LOG(LS_INFO) << "Configure: mime=" << mime_ << ", media_type="
                   << static_cast<int>(config->info.media_type)
                   << ", is_encoder=" << is_encoder_;

  AMediaFormat* ndk_format = MediaMetaToAMediaFormat(config->format);
  if (!ndk_format) {
    AVE_LOG(LS_ERROR) << "Configure: failed to create AMediaFormat";
    return UNKNOWN_ERROR;
  }

  // For video decoders, we don't pass a surface for now (decode to buffer).
  // TODO(youfa): Support surface-based rendering with ANativeWindow.
  media_status_t status = AMediaCodec_configure(
      android_media_codec_, ndk_format,
      nullptr,  // surface (ANativeWindow*)
      nullptr,  // crypto
      is_encoder_ ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0);

  AMediaFormat_delete(ndk_format);

  if (status != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "Configure: AMediaCodec_configure failed, status="
                      << status;
    return MapMediaStatus(status);
  }

  AVE_LOG(LS_INFO) << "Configure: success";
  return OK;
}

status_t AndroidNdkMediaCodec::SetCallback(CodecCallback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "SetCallback: codec not created";
    return NO_INIT;
  }

  callback_ = callback;
  AVE_LOG(LS_VERBOSE) << "SetCallback: setting async callbacks";

  if (__builtin_available(android 28, *)) {
    media_status_t status = AMediaCodec_setAsyncNotifyCallback(
        android_media_codec_, AMediaCodecCallbackHelper{}, this);
    if (status != AMEDIA_OK) {
      AVE_LOG(LS_ERROR) << "SetCallback: setAsyncNotifyCallback failed, status="
                        << status;
      return MapMediaStatus(status);
    }
  } else {
    AVE_LOG(LS_ERROR) << "SetCallback: async callbacks require API 28+";
    return ERROR_UNSUPPORTED;
  }

  if (__builtin_available(android 33, *)) {
    media_status_t status = AMediaCodec_setOnFrameRenderedCallback(
        android_media_codec_, AMediaCodecCallbackHelper::onFrameRendered, this);
    if (status != AMEDIA_OK) {
      AVE_LOG(LS_WARNING) << "SetCallback: setOnFrameRenderedCallback failed"
                          << " (non-fatal), status=" << status;
    }
  }

  return OK;
}

status_t AndroidNdkMediaCodec::Start() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "Start: codec not created";
    return NO_INIT;
  }

  AVE_LOG(LS_INFO) << "Start: starting AMediaCodec";
  media_status_t status = AMediaCodec_start(android_media_codec_);
  if (status != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "Start: AMediaCodec_start failed, status=" << status;
    return MapMediaStatus(status);
  }

  AVE_LOG(LS_INFO) << "Start: success";
  return OK;
}

status_t AndroidNdkMediaCodec::Stop() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    AVE_LOG(LS_WARNING) << "Stop: codec not created";
    return NO_INIT;
  }

  AVE_LOG(LS_INFO) << "Stop: stopping AMediaCodec";
  media_status_t status = AMediaCodec_stop(android_media_codec_);
  if (status != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "Stop: AMediaCodec_stop failed, status=" << status;
  }

  // Clear buffer maps
  input_buffers_.clear();
  output_buffers_.clear();
  output_buffer_infos_.clear();

  return MapMediaStatus(status);
}

status_t AndroidNdkMediaCodec::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return NO_INIT;
  }

  AVE_LOG(LS_INFO) << "Flush: flushing AMediaCodec";
  media_status_t status = AMediaCodec_flush(android_media_codec_);

  input_buffers_.clear();
  output_buffers_.clear();
  output_buffer_infos_.clear();

  return MapMediaStatus(status);
}

status_t AndroidNdkMediaCodec::Reset() {
  AVE_LOG(LS_INFO) << "Reset";
  Stop();
  return OK;
}

status_t AndroidNdkMediaCodec::Release() {
  AVE_LOG(LS_INFO) << "Release";
  Stop();
  return OK;
}

status_t AndroidNdkMediaCodec::GetInputBuffer(
    size_t index,
    std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return NO_INIT;
  }

  size_t size = 0;
  auto* addr = AMediaCodec_getInputBuffer(android_media_codec_, index, &size);
  if (!addr) {
    AVE_LOG(LS_ERROR) << "GetInputBuffer: null buffer for index=" << index;
    return ERROR_RETRY;
  }

  AVE_LOG(LS_VERBOSE) << "GetInputBuffer: index=" << index
                      << ", capacity=" << size;

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

  if (!android_media_codec_) {
    return NO_INIT;
  }

  size_t total_size = 0;
  auto* addr =
      AMediaCodec_getOutputBuffer(android_media_codec_, index, &total_size);
  if (!addr) {
    AVE_LOG(LS_ERROR) << "GetOutputBuffer: null buffer for index=" << index;
    return UNKNOWN_ERROR;
  }

  // Use the actual data range from bufferInfo
  int32_t data_offset = 0;
  int32_t data_size = static_cast<int32_t>(total_size);
  auto info_it = output_buffer_infos_.find(index);
  if (info_it != output_buffer_infos_.end()) {
    data_offset = info_it->second.offset;
    data_size = info_it->second.size;
  }

  AVE_LOG(LS_VERBOSE) << "GetOutputBuffer: index=" << index
                      << ", total_size=" << total_size
                      << ", data_offset=" << data_offset
                      << ", data_size=" << data_size;

  auto codec_buffer =
      std::make_shared<CodecBuffer>(addr + data_offset, data_size);

  // Attach PTS metadata from the output buffer info
  if (info_it != output_buffer_infos_.end()) {
    auto meta = MediaMeta::CreatePtr(MediaType::UNKNOWN,
                                     MediaMeta::FormatType::kSample);
    meta->SetPts(base::Timestamp::Micros(info_it->second.presentation_time_us));
    codec_buffer->format() = meta;
  }

  output_buffers_[index] = codec_buffer;
  buffer = codec_buffer;
  return OK;
}

ssize_t AndroidNdkMediaCodec::DequeueInputBuffer(int64_t timeoutUs) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return -1;
  }

  auto index = AMediaCodec_dequeueInputBuffer(android_media_codec_, timeoutUs);
  AVE_LOG(LS_VERBOSE) << "DequeueInputBuffer: index=" << index;
  return static_cast<ssize_t>(index);
}

status_t AndroidNdkMediaCodec::QueueInputBuffer(size_t index) {
  // Do NOT hold the mutex_ here — AMediaCodec_queueInputBuffer may trigger
  // async callbacks that also try to lock mutex_.
  std::shared_ptr<CodecBuffer> input_buffer;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!android_media_codec_) {
      return NO_INIT;
    }
    auto it = input_buffers_.find(index);
    if (it == input_buffers_.end()) {
      AVE_LOG(LS_ERROR) << "QueueInputBuffer: no buffer for index=" << index;
      return INVALID_OPERATION;
    }
    input_buffer = it->second;
  }

  auto offset = input_buffer->offset();
  auto size = input_buffer->size();
  int64_t pts = 0;
  if (input_buffer->format()) {
    pts = input_buffer->format()->pts().us_or(0);
  }

  uint32_t flags = 0;
  if (input_buffer->format() && input_buffer->format()->eos()) {
    flags = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
  }

  AVE_LOG(LS_VERBOSE) << "QueueInputBuffer: index=" << index
                      << ", offset=" << offset << ", size=" << size
                      << ", pts=" << pts << ", flags=" << flags;

  media_status_t ret = AMediaCodec_queueInputBuffer(android_media_codec_, index,
                                                    static_cast<off_t>(offset),
                                                    size, pts, flags);

  if (ret != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "QueueInputBuffer: failed, status=" << ret;
  }

  return MapMediaStatus(ret);
}

ssize_t AndroidNdkMediaCodec::DequeueOutputBuffer(int64_t timeout_us) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return -1;
  }

  AMediaCodecBufferInfo info{};
  auto index =
      AMediaCodec_dequeueOutputBuffer(android_media_codec_, &info, timeout_us);
  if (index >= 0) {
    output_buffer_infos_[static_cast<size_t>(index)] = {
        info.offset, info.size, info.presentationTimeUs,
        static_cast<uint32_t>(info.flags)};
    AVE_LOG(LS_VERBOSE) << "DequeueOutputBuffer: index=" << index
                        << ", size=" << info.size
                        << ", pts=" << info.presentationTimeUs;
  }

  return static_cast<ssize_t>(index);
}

status_t AndroidNdkMediaCodec::ReleaseOutputBuffer(size_t index, bool render) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return NO_INIT;
  }

  AVE_LOG(LS_VERBOSE) << "ReleaseOutputBuffer: index=" << index
                      << ", render=" << render;

  media_status_t ret =
      AMediaCodec_releaseOutputBuffer(android_media_codec_, index, render);

  output_buffers_.erase(index);
  output_buffer_infos_.erase(index);

  if (ret != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "ReleaseOutputBuffer: failed, status=" << ret;
  }

  return MapMediaStatus(ret);
}

void AndroidNdkMediaCodec::NotifyInputBufferAvailable(size_t index) {
  // Do NOT lock mutex_ here — callback_ methods may post messages that
  // eventually call back into codec methods that acquire the lock.
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnInputBufferAvailable(index);
  }
}

void AndroidNdkMediaCodec::NotifyOutputBufferAvailable(
    size_t index,
    int32_t offset,
    int32_t size,
    int64_t presentation_time_us,
    uint32_t flags) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    output_buffer_infos_[index] = {offset, size, presentation_time_us, flags};
  }
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnOutputBufferAvailable(index);
  }
}

void AndroidNdkMediaCodec::NotifyOutputFormatChanged(AMediaFormat* format) {
  auto meta = MediaMeta::CreatePtr();
  if (format) {
    // Extract key fields from the new format
    int32_t width = 0, height = 0, sample_rate = 0, channel_count = 0;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width) &&
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height)) {
      meta->SetWidth(width);
      meta->SetHeight(height);
      AVE_LOG(LS_INFO) << "OutputFormatChanged: video " << width << "x"
                       << height;
    }
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                              &sample_rate)) {
      meta->SetSampleRate(static_cast<uint32_t>(sample_rate));
    }
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                              &channel_count)) {
      meta->SetChannelLayout(GuessChannelLayout(channel_count));
    }
  }

  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnOutputFormatChanged(meta);
  }
}

void AndroidNdkMediaCodec::NotifyError(status_t error) {
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnError(error);
  }
}

void AndroidNdkMediaCodec::NotifyFrameRendered(
    std::shared_ptr<Message> message) {
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnFrameRendered(message);
  }
}

}  // namespace media
}  // namespace ave
