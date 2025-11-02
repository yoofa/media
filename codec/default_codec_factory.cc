/*
 * default_codec_factory.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "default_codec_factory.h"

#include "base/logging.h"

namespace ave {
namespace media {

DefaultCodecFactory::DefaultCodecFactory() {
  software_factory_ = std::make_shared<SoftwareCodecFactory>();
  hardware_factory_ = std::make_shared<HardwareCodecFactory>();
}

DefaultCodecFactory::~DefaultCodecFactory() {}

std::vector<CodecInfo> DefaultCodecFactory::GetSupportedCodecs() {
  std::vector<CodecInfo> codecs;
  auto hw_codecs = hardware_factory_->GetSupportedCodecs();
  codecs.insert(codecs.end(), hw_codecs.begin(), hw_codecs.end());

  auto sw_codecs = software_factory_->GetSupportedCodecs();
  codecs.insert(codecs.end(), sw_codecs.begin(), sw_codecs.end());
  return codecs;
}

std::shared_ptr<Codec> DefaultCodecFactory::CreateCodecByType(CodecId codec_id,
                                                              bool encoder) {
  // Try hardware first
  auto codec = hardware_factory_->CreateCodecByType(codec_id, encoder);
  if (codec) {
    AVE_LOG(LS_INFO) << "Created hardware codec for "
                     << static_cast<int>(codec_id);
    return codec;
  }

  // Fallback to software
  codec = software_factory_->CreateCodecByType(codec_id, encoder);
  if (codec) {
    AVE_LOG(LS_INFO) << "Created software codec for "
                     << static_cast<int>(codec_id);
    return codec;
  }

  AVE_LOG(LS_ERROR) << "Failed to create codec for "
                    << static_cast<int>(codec_id);
  return nullptr;
}

std::shared_ptr<Codec> DefaultCodecFactory::CreateCodecByName(
    const std::string& name) {
  auto codec = hardware_factory_->CreateCodecByName(name);
  if (codec) {
    return codec;
  }
  return software_factory_->CreateCodecByName(name);
}

std::shared_ptr<Codec> DefaultCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  auto codec = hardware_factory_->CreateCodecByMime(mime, encoder);
  if (codec) {
    return codec;
  }
  return software_factory_->CreateCodecByMime(mime, encoder);
}

}  // namespace media
}  // namespace ave
