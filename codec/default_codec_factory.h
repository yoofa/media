/*
 * default_codec_factory.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef DEFAULT_CODEC_FACTORY_H
#define DEFAULT_CODEC_FACTORY_H

#include "codec_factory.h"
#include "hardware_codec_factory.h"
#include "software_codec_factory.h"

namespace ave {
namespace media {

class DefaultCodecFactory : public CodecFactory {
 public:
  DefaultCodecFactory();
  ~DefaultCodecFactory() override;

  std::vector<CodecInfo> GetSupportedCodecs() override;
  std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id,
                                           bool encoder) override;
  std::shared_ptr<Codec> CreateCodecByName(const std::string& name) override;
  std::shared_ptr<Codec> CreateCodecByMime(const std::string& mime,
                                           bool encoder) override;

  std::string name() const override { return "default"; }
  int16_t priority() const override { return 0; }

 private:
  std::shared_ptr<SoftwareCodecFactory> software_factory_;
  std::shared_ptr<HardwareCodecFactory> hardware_factory_;
};

}  // namespace media
}  // namespace ave

#endif /* !DEFAULT_CODEC_FACTORY_H */
