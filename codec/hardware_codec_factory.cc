/*
 * hardware_codec_factory.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "hardware_codec_factory.h"

#include "base/logging.h"

namespace ave {
namespace media {

HardwareCodecFactory::HardwareCodecFactory() {}

HardwareCodecFactory::~HardwareCodecFactory() {}

std::vector<CodecInfo> HardwareCodecFactory::GetSupportedCodecs() {
  return {};
}

std::shared_ptr<Codec> HardwareCodecFactory::CreateCodecByType(CodecId codec_id,
                                                               bool encoder) {
  return nullptr;
}

std::shared_ptr<Codec> HardwareCodecFactory::CreateCodecByName(
    const std::string& name) {
  return nullptr;
}

std::shared_ptr<Codec> HardwareCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  return nullptr;
}

}  // namespace media
}  // namespace ave
