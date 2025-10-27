/*
 * simple_passthrough_codec.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef SIMPLE_PASSTHROUGH_CODEC_H
#define SIMPLE_PASSTHROUGH_CODEC_H

#include "simple_codec.h"

namespace ave {
namespace media {

/**
 * SimplePassthroughCodec - A simple passthrough codec implementation
 *
 * This codec does no actual encoding or decoding. Data goes in and comes out
 * unchanged. It's useful for:
 * - Testing the codec framework
 * - Benchmarking codec overhead
 * - Template for implementing other simple codecs
 *
 * The workflow is simple:
 * Input buffer -> Copy to output buffer -> Done
 */
class SimplePassthroughCodec : public SimpleCodec {
 public:
  explicit SimplePassthroughCodec(bool is_encoder);
  ~SimplePassthroughCodec() override;

 protected:
  status_t OnConfigure(const std::shared_ptr<CodecConfig>& config)
      REQUIRES(task_runner_) override;
  status_t OnStart() REQUIRES(task_runner_) override;
  status_t OnStop() REQUIRES(task_runner_) override;
  status_t OnReset() REQUIRES(task_runner_) override;
  status_t OnFlush() REQUIRES(task_runner_) override;
  status_t OnRelease() REQUIRES(task_runner_) override;

  void ProcessInput(size_t index) REQUIRES(task_runner_) override;
  void ProcessOutput() REQUIRES(task_runner_) override;

 private:
  // No internal state needed for passthrough
  int64_t frame_count_ GUARDED_BY(task_runner_);
};

}  // namespace media
}  // namespace ave

#endif /* !SIMPLE_PASSTHROUGH_CODEC_H */
