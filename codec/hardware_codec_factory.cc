/*
 * hardware_codec_factory.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "hardware_codec_factory.h"

#include "base/logging.h"

#if defined(AVE_ANDROID)
#include "android/android_ndk_media_codec_factory.h"
#endif

namespace ave {
namespace media {

HardwareCodecFactory::HardwareCodecFactory() {
#if defined(AVE_ANDROID)
  platform_factory_ = std::make_shared<AndroidNdkMediaCodecFactory>();
  AVE_LOG(LS_INFO) << "HardwareCodecFactory: using AndroidNdkMediaCodecFactory";
#else
  AVE_LOG(LS_INFO)
      << "HardwareCodecFactory: no platform hardware codec available";
#endif
}

HardwareCodecFactory::~HardwareCodecFactory() {}

std::vector<CodecInfo> HardwareCodecFactory::GetSupportedCodecs() {
  if (platform_factory_) {
    return platform_factory_->GetSupportedCodecs();
  }
  return {};
}

std::shared_ptr<Codec> HardwareCodecFactory::CreateCodecByType(CodecId codec_id,
                                                               bool encoder) {
  if (platform_factory_) {
    auto codec = platform_factory_->CreateCodecByType(codec_id, encoder);
    if (codec) {
      AVE_LOG(LS_INFO) << "HardwareCodecFactory: created codec by type "
                       << static_cast<int>(codec_id);
      return codec;
    }
    AVE_LOG(LS_VERBOSE)
        << "HardwareCodecFactory: platform factory failed for type "
        << static_cast<int>(codec_id);
  }
  return nullptr;
}

std::shared_ptr<Codec> HardwareCodecFactory::CreateCodecByName(
    const std::string& name) {
  if (platform_factory_) {
    auto codec = platform_factory_->CreateCodecByName(name);
    if (codec) {
      AVE_LOG(LS_INFO) << "HardwareCodecFactory: created codec by name: "
                       << name;
      return codec;
    }
  }
  return nullptr;
}

std::shared_ptr<Codec> HardwareCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  if (platform_factory_) {
    auto codec = platform_factory_->CreateCodecByMime(mime, encoder);
    if (codec) {
      AVE_LOG(LS_INFO) << "HardwareCodecFactory: created codec by mime: "
                       << mime;
      return codec;
    }
  }
  return nullptr;
}

}  // namespace media
}  // namespace ave
