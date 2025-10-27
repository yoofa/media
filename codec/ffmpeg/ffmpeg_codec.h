/*
 * ffmpeg_codec.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef FFMPEG_CODEC_H
#define FFMPEG_CODEC_H

#include "media/codec/simple_codec.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace ave {
namespace media {

class FFmpegCodec : public SimpleCodec {
 public:
  explicit FFmpegCodec(const AVCodec* codec, bool is_encoder);
  ~FFmpegCodec() override;

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
  const AVCodec* codec_;
  AVCodecContext* codec_ctx_ GUARDED_BY(task_runner_);
};

}  // namespace media
}  // namespace ave

#endif /* !FFMPEG_CODEC_H */
