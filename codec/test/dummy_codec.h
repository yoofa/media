/*
 * dummy_codec.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_CODEC_TEST_DUMMY_CODEC_H_
#define MEDIA_CODEC_TEST_DUMMY_CODEC_H_

#include <queue>
#include "base/task_util/task_runner.h"
#include "media/codec/codec.h"

namespace ave {
namespace media {
namespace test {

class DummyCodec : public Codec {
 public:
  explicit DummyCodec(bool is_encoder);
  ~DummyCodec() override;

  // Codec implementation
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

 private:
  enum class BufferState {
    FREE,     // Buffer is available for use
    PENDING,  // Buffer is notified but not yet acquired
    INUSE     // Buffer is being used
  };

  struct BufferEntry {
    BufferState state{BufferState::FREE};
    std::shared_ptr<CodecBuffer> buffer;
  };

  // Helper functions for buffer state management
  bool IsBufferAvailable(const BufferEntry& entry) const {
    return entry.state == BufferState::FREE;
  }

  bool IsBufferPending(const BufferEntry& entry) const {
    return entry.state == BufferState::PENDING;
  }

  bool IsBufferInUse(const BufferEntry& entry) const {
    return entry.state == BufferState::INUSE;
  }

  void RequestInputBuffer();
  void ProcessBuffer(size_t input_index);
  std::shared_ptr<CodecBuffer> FindAvailableInputBuffer(int32_t index);
  std::shared_ptr<CodecBuffer> FindAvailableOutputBuffer();

  const bool is_encoder_;
  std::mutex lock_;
  std::condition_variable cv_;

  CodecCallback* callback_ = nullptr;
  std::vector<BufferEntry> input_buffers_;
  std::vector<BufferEntry> output_buffers_;
  std::queue<size_t> output_queue_;

  std::unique_ptr<base::TaskRunner> task_runner_;
  bool started_ = false;
  std::shared_ptr<CodecConfig> config_;
};

}  // namespace test
}  // namespace media
}  // namespace ave

#endif  // MEDIA_CODEC_TEST_DUMMY_CODEC_H_
