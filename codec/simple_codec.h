/*
 * simple_codec.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef SIMPLE_CODEC_H
#define SIMPLE_CODEC_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "base/task_util/task_runner.h"
#include "base/thread_annotation.h"
#include "codec.h"

namespace ave {
namespace media {

class SimpleCodec : public Codec {
 public:
  explicit SimpleCodec(bool is_encoder);
  ~SimpleCodec() override;

  status_t Configure(const std::shared_ptr<CodecConfig>& config) override;
  status_t SetCallback(CodecCallback* callback) override;
  status_t Start() override;
  status_t Stop() override;
  status_t Reset() override;
  status_t Flush() override;
  status_t Release() override;

  status_t GetInputBuffer(size_t index,
                          std::shared_ptr<CodecBuffer>& buffer) override;
  status_t GetOutputBuffer(size_t index,
                           std::shared_ptr<CodecBuffer>& buffer) override;

  ssize_t DequeueInputBuffer(int64_t timeout_ms)
      NO_THREAD_SAFETY_ANALYSIS override;
  status_t QueueInputBuffer(size_t index) override;
  ssize_t DequeueOutputBuffer(int64_t timeout_ms)
      NO_THREAD_SAFETY_ANALYSIS override;
  status_t ReleaseOutputBuffer(size_t index, bool render) override;

  bool IsEncoder() const { return is_encoder_; }

 protected:
  enum class State {
    UNINITIALIZED,
    CONFIGURED,
    STARTED,
    STOPPED,
    ERROR,
    RELEASED
  };

  struct BufferEntry {
    bool in_use{false};
    std::shared_ptr<CodecBuffer> buffer;
  };

  virtual status_t OnConfigure(const std::shared_ptr<CodecConfig>& config)
      REQUIRES(task_runner_) = 0;
  virtual status_t OnStart() REQUIRES(task_runner_) = 0;
  virtual status_t OnStop() REQUIRES(task_runner_) = 0;
  virtual status_t OnReset() REQUIRES(task_runner_) = 0;
  virtual status_t OnFlush() REQUIRES(task_runner_) = 0;
  virtual status_t OnRelease() REQUIRES(task_runner_) = 0;

  virtual void ProcessInput(size_t index) REQUIRES(task_runner_) = 0;
  virtual void ProcessOutput() REQUIRES(task_runner_) = 0;

  void Process() REQUIRES(task_runner_);
  void NotifyInputBufferAvailable(size_t index) REQUIRES(task_runner_);
  void NotifyOutputBufferAvailable(size_t index) REQUIRES(task_runner_);
  void NotifyOutputFormatChanged(const std::shared_ptr<MediaMeta>& format)
      REQUIRES(task_runner_);
  void NotifyError(status_t error) REQUIRES(task_runner_);

  size_t GetAvailableOutputBufferIndex() REQUIRES(lock_);
  size_t PushOutputBuffer(size_t index) REQUIRES(lock_);

  const bool is_encoder_;
  std::unique_ptr<base::TaskRunner> task_runner_;
  std::mutex lock_;
  std::condition_variable cv_;

  State state_ GUARDED_BY(task_runner_);
  CodecCallback* callback_ GUARDED_BY(task_runner_);
  std::shared_ptr<CodecConfig> config_ GUARDED_BY(task_runner_);

  std::vector<BufferEntry> input_buffers_ GUARDED_BY(lock_);
  std::vector<BufferEntry> output_buffers_ GUARDED_BY(lock_);
  std::queue<size_t> input_queue_ GUARDED_BY(lock_);
  std::queue<size_t> output_queue_ GUARDED_BY(lock_);
};

}  // namespace media
}  // namespace ave

#endif /* !SIMPLE_CODEC_H */
