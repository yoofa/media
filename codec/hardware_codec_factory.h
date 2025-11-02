/*
 * hardware_codec_factory.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef HARDWARE_CODEC_FACTORY_H
#define HARDWARE_CODEC_FACTORY_H

#include "codec_factory.h"

namespace ave {
namespace media {

class HardwareCodecFactory : public CodecFactory {
 public:
  HardwareCodecFactory();
  ~HardwareCodecFactory() override;

  std::vector<CodecInfo> GetSupportedCodecs() override;
  std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id,
                                           bool encoder) override;
  std::shared_ptr<Codec> CreateCodecByName(const std::string& name) override;
  std::shared_ptr<Codec> CreateCodecByMime(const std::string& mime,
                                           bool encoder) override;

  std::string name() const override { return "hardware"; }
  int16_t priority() const override { return 200; }
};

}  // namespace media
}  // namespace ave

#endif /* !HARDWARE_CODEC_FACTORY_H */
