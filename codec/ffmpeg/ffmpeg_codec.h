/*
 * ffmpeg_codec.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef FFMPEG_CODEC_H
#define FFMPEG_CODEC_H

#include <condition_variable>
#include "base/task_util/task_runner.h"
#include "base/thread_annotation.h"
#include "media/codec/codec.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace ave {
namespace media {

class FFmpegCodec : public Codec {
 public:
  explicit FFmpegCodec(const AVCodec* codec, bool is_encoder);
  ~FFmpegCodec() override;

  status_t Configure(const std::shared_ptr<CodecConfig>& config) override;
  status_t SetCallback(CodecCallback* callback) override;
  status_t Start() override;
  status_t Stop() override;
  status_t Reset() override;
  status_t Flush() override;
  status_t Release() override;

  std::shared_ptr<CodecBuffer> DequeueInputBuffer(int32_t index,
                                                  int64_t timeout_ms) override;
  status_t QueueInputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                            int64_t timeout_ms) override;
  std::shared_ptr<CodecBuffer> DequeueOutputBuffer(int32_t index,
                                                   int64_t timeout_ms) override;
  status_t ReleaseOutputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                               bool render) override;

  bool IsEncoder() const { return is_encoder_; }

  void Process() REQUIRES(task_runner_);
  void MaybeSendPacket() REQUIRES(task_runner_);
  void MaybeReceiveFrame() REQUIRES(task_runner_);
  void OnInputBufferAvailable(size_t index) REQUIRES(task_runner_);
  void OnOutputBufferAvailable(size_t index) REQUIRES(task_runner_);
  void OnError(status_t error) REQUIRES(task_runner_);

 private:
  struct BufferEntry {
    bool in_use{false};
    std::shared_ptr<CodecBuffer> buffer;
  };

  const AVCodec* codec_;
  const bool is_encoder_;
  std::unique_ptr<base::TaskRunner> task_runner_;
  std::mutex lock_;
  std::condition_variable cv_;

  AVCodecContext* codec_ctx_ GUARDED_BY(task_runner_);
  CodecCallback* callback_ GUARDED_BY(task_runner_);
  std::vector<BufferEntry> input_buffers_ GUARDED_BY(lock_);
  std::vector<BufferEntry> output_buffers_ GUARDED_BY(lock_);
  // input buffer queue pending for processing
  std::queue<size_t> input_queue_ GUARDED_BY(lock_);
  // output buffer queue after processing
  std::queue<size_t> output_queue_ GUARDED_BY(lock_);
};

}  // namespace media
}  // namespace ave

#endif /* !FFMPEG_CODEC_H */
