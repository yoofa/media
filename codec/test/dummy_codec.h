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

/**
 * @brief A dummy codec for testing that simply copies input to output.
 *
 * Implements the current Codec interface with index-based buffer management.
 * Data passes through unchanged - useful for testing the codec framework
 * and pipeline without actual encoding/decoding.
 */
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

  status_t GetInputBuffer(size_t index,
                          std::shared_ptr<CodecBuffer>& buffer) override;
  status_t GetOutputBuffer(size_t index,
                           std::shared_ptr<CodecBuffer>& buffer) override;

  ssize_t DequeueInputBuffer(int64_t timeout_ms) override;
  status_t QueueInputBuffer(size_t index) override;
  ssize_t DequeueOutputBuffer(int64_t timeout_ms) override;
  status_t ReleaseOutputBuffer(size_t index, bool render) override;

 private:
  struct BufferEntry {
    bool in_use{false};
    std::shared_ptr<CodecBuffer> buffer;
  };

  void ProcessBuffer(size_t input_index);

  const bool is_encoder_;
  std::mutex lock_;
  std::condition_variable cv_;

  CodecCallback* callback_ = nullptr;
  std::vector<BufferEntry> input_buffers_;
  std::vector<BufferEntry> output_buffers_;
  std::queue<size_t> input_queue_;
  std::queue<size_t> output_queue_;

  std::unique_ptr<base::TaskRunner> task_runner_;
  bool started_ = false;
  std::shared_ptr<CodecConfig> config_;
};

}  // namespace test
}  // namespace media
}  // namespace ave

#endif  // MEDIA_CODEC_TEST_DUMMY_CODEC_H_
