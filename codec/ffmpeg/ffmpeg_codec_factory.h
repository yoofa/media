/*
 * ffmpeg_codec_factory.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef FFMPEG_CODEC_FACTORY_H
#define FFMPEG_CODEC_FACTORY_H

#include "../codec_factory.h"

namespace ave {
namespace media {

class FFmpegCodecFactory : public CodecFactory {
 public:
  FFmpegCodecFactory();
  ~FFmpegCodecFactory() override;

  std::vector<CodecInfo> GetSupportedCodecs() override;
  std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id,
                                           bool encoder) override;
  std::shared_ptr<Codec> CreateCodecByName(const std::string& name) override;
  std::shared_ptr<Codec> CreateCodecByMime(const std::string& mime,
                                           bool encoder) override;

  std::string name() const override { return "ffmpeg"; }
  int16_t priority() const override {
    return 100;
  }  // Adjust priority as needed

 private:
  std::vector<CodecInfo> supported_codecs_;
};

}  // namespace media
}  // namespace ave

#endif /* !FFMPEG_CODEC_FACTORY_H */
