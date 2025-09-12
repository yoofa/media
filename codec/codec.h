/*
 * codec.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef CODEC_H
#define CODEC_H

#include <memory>

#include "base/errors.h"

#include "../crypto/crypto.h"
#include "../foundation/media_meta.h"
#include "../foundation/media_utils.h"
#include "../foundation/message.h"
#include "../video/video_render.h"

#include "codec_buffer.h"

namespace ave {
namespace media {

struct CodecInfo {
  std::string name;
  std::string mime;
  MediaType media_type = MediaType::UNKNOWN;

  // TODO: support resolution, frame rate, bit rate, etc

  bool is_encoder = false;
  bool hardware_accelerated = false;
};

// callback for codec
class CodecCallback {
 public:
  virtual ~CodecCallback() = default;
  // fill the input buffer
  virtual void OnInputBufferAvailable(size_t index) = 0;
  // output buffer is available
  virtual void OnOutputBufferAvailable(size_t index) = 0;
  // output format is changed
  virtual void OnOutputFormatChanged(
      const std::shared_ptr<MediaMeta>& format) = 0;
  // error happened
  virtual void OnError(status_t error) = 0;
  // frame is rendered
  virtual void OnFrameRendered(std::shared_ptr<Message> notify) = 0;
};

// codec configuration
struct CodecConfig {
  CodecInfo info;
  std::shared_ptr<VideoRender> video_render;
  std::shared_ptr<Crypto> crypto;
  std::shared_ptr<MediaMeta> format;
};

// this class is porting from Android MediaCodec
// Design constraint:
// Some codecs are very particular about their buffers. They may need to have a
// particular memory alignment,
//  or have a certain minimum or maximum size, or it may be important to have a
//  certain number of them available. To accommodate the wide range of
//  possibilities, buffer allocation is performed by the codecs themselves,
//  rather than the application. You do not hand a buffer with data to Codec.
//  You ask it for a buffer, and if one is available, you copy the data in.
class Codec {
 public:
  virtual ~Codec() = default;

  // video_render: decoder -> video render path
  virtual status_t Configure(const std::shared_ptr<CodecConfig>& config) = 0;

  virtual status_t SetCallback(CodecCallback* callback) = 0;

  // running operation
  virtual status_t Start() = 0;
  virtual status_t Stop() = 0;
  virtual status_t Reset() = 0;
  virtual status_t Flush() = 0;
  virtual status_t Release() = 0;

  /*
   * get input buffers refs from codec, can be used after Configure
   * size maybe changed when running
   */
  virtual std::vector<std::shared_ptr<CodecBuffer>> InputBuffers() = 0;

  /*
   * get output buffers refs from codec, can be used after Configure
   * size maybe changed when running
   */
  virtual std::vector<std::shared_ptr<CodecBuffer>> OutputBuffers() = 0;

  /*
   * get input buffer from codec, can be used after DequeueInputBuffer
   * can be used to fill the buffer
   */
  virtual status_t GetInputBuffer(size_t index,
                                  std::shared_ptr<CodecBuffer>& buffer) = 0;
  /*
   * get output buffer from codec, can be used after DequeueOutputBuffer
   * can be used to render the buffer
   */
  virtual status_t GetOutputBuffer(size_t index,
                                   std::shared_ptr<CodecBuffer>& buffer) = 0;

  /*
   * get input buffer from codec, will wait until timeout_ms
   * return index or status_t::E_AGAIN if no input buffer available
   */
  virtual ssize_t DequeueInputBuffer(int64_t timeout_ms) = 0;

  /* queueInputBuffer to codec, will copy the data to codec internal buffer
   * index: input buffer index, can be got from DequeueInputBuffer or callback
   * OnInputBufferAvailable
   * return status_t::E_AGAIN if the codec is not ready or input queue in full
   */
  status_t QueueInputBuffer(size_t index) {
    return QueueInputBuffer(index, -1);
  }
  virtual status_t QueueInputBuffer(size_t index, int64_t timeout_ms) = 0;

  /*
   * get output buffer from codec, will wait until timeout_ms
   * return index or status_t::E_AGAIN if no output buffer available
   */
  virtual ssize_t DequeueOutputBuffer(int64_t timeout_ms) = 0;

  /* release the output buffer
   * index: output buffer index, can be got from DequeueOutputBuffer or
   * callback OnOutputBufferAvailable
   * render: true - render the buffer, false - only release the buffer
   */
  virtual status_t ReleaseOutputBuffer(size_t index, bool render) = 0;
};

}  // namespace media
}  // namespace ave

#endif /* !CODEC_H */
