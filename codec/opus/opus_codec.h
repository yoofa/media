/*
 * opus_codec.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_OPUS_CODEC_H
#define AVE_MEDIA_OPUS_CODEC_H

#include "media/codec/simple_codec.h"

extern "C" {
#include <opus.h>
}

namespace ave {
namespace media {

class OpusCodec : public SimpleCodec {
 public:
  explicit OpusCodec(bool is_encoder);
  ~OpusCodec() override;

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
  union {
    OpusEncoder* encoder;
    OpusDecoder* decoder;
  } opus_ctx_ GUARDED_BY(task_runner_);

  int32_t sample_rate_ GUARDED_BY(task_runner_);
  int32_t channels_ GUARDED_BY(task_runner_);
  int32_t frame_size_ GUARDED_BY(task_runner_);
  int32_t complexity_ GUARDED_BY(task_runner_);
  int32_t signal_type_ GUARDED_BY(task_runner_);
};

}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_OPUS_CODEC_H */
