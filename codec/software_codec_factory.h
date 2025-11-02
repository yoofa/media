/*
 * software_codec_factory.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef SOFTWARE_CODEC_FACTORY_H
#define SOFTWARE_CODEC_FACTORY_H

#include "codec_factory.h"
#include "ffmpeg/ffmpeg_codec_factory.h"

namespace ave {
namespace media {

class SoftwareCodecFactory : public CodecFactory {
 public:
  SoftwareCodecFactory();
  ~SoftwareCodecFactory() override;

  std::vector<CodecInfo> GetSupportedCodecs() override;
  std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id,
                                           bool encoder) override;
  std::shared_ptr<Codec> CreateCodecByName(const std::string& name) override;
  std::shared_ptr<Codec> CreateCodecByMime(const std::string& mime,
                                           bool encoder) override;

  std::string name() const override { return "software"; }
  int16_t priority() const override { return 100; }

 private:
  std::shared_ptr<FFmpegCodecFactory> ffmpeg_factory_;
};

}  // namespace media
}  // namespace ave

#endif /* !SOFTWARE_CODEC_FACTORY_H */
