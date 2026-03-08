/*
 * android_ndk_media_codec_factory.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "android_ndk_media_codec_factory.h"

#include <memory>

#include "base/logging.h"

#include "android_ndk_media_codec.h"

namespace ave {

namespace media {

AndroidNdkMediaCodecFactory::AndroidNdkMediaCodecFactory() {
  AVE_LOG(LS_INFO) << "AndroidNdkMediaCodecFactory: created";
}

AndroidNdkMediaCodecFactory::~AndroidNdkMediaCodecFactory() = default;

std::vector<CodecInfo> AndroidNdkMediaCodecFactory::GetSupportedCodecs() {
  return supported_codecs_;
}

std::shared_ptr<Codec> AndroidNdkMediaCodecFactory::CreateCodecByType(
    CodecId codec_id,
    bool encoder) {
  const auto* mime = CodecIdToMime(codec_id);
  if (!mime || mime[0] == '\0') {
    AVE_LOG(LS_WARNING) << "AndroidNdkMediaCodecFactory: no mime for codec_id="
                        << static_cast<int>(codec_id);
    return nullptr;
  }
  AVE_LOG(LS_INFO)
      << "AndroidNdkMediaCodecFactory: CreateCodecByType, codec_id="
      << static_cast<int>(codec_id) << ", mime=" << mime
      << ", encoder=" << encoder;
  return CreateCodecByMime(mime, encoder);
}

std::shared_ptr<Codec> AndroidNdkMediaCodecFactory::CreateCodecByName(
    const std::string& name) {
  AVE_LOG(LS_INFO) << "AndroidNdkMediaCodecFactory: CreateCodecByName: "
                   << name;
  auto codec = std::make_shared<AndroidNdkMediaCodec>(name.c_str());
  return codec;
}

std::shared_ptr<Codec> AndroidNdkMediaCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  AVE_LOG(LS_INFO) << "AndroidNdkMediaCodecFactory: CreateCodecByMime: " << mime
                   << ", encoder=" << encoder;
  auto codec = std::make_shared<AndroidNdkMediaCodec>(
      mime.c_str(), AndroidNdkMediaCodec::CreatedBy::kCreatedByMime, encoder);
  return codec;
}

}  // namespace media

}  // namespace ave
