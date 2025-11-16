/*
 * android_ndk_media_codec_factory.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "android_ndk_media_codec_factory.h"

#include <memory>

#include "android_ndk_media_codec.h"

namespace ave {

namespace media {

AndroidNdkMediaCodecFactory::AndroidNdkMediaCodecFactory() = default;
AndroidNdkMediaCodecFactory::~AndroidNdkMediaCodecFactory() = default;

std::vector<CodecInfo> AndroidNdkMediaCodecFactory::GetSupportedCodecs() {
  return supported_codecs_;
}

std::shared_ptr<Codec> AndroidNdkMediaCodecFactory::CreateCodecByType(
    CodecId codec_id,
    bool encoder) {
  const auto* mime = CodecIdToMime(codec_id);
  return CreateCodecByMime(mime, encoder);
}

std::shared_ptr<Codec> AndroidNdkMediaCodecFactory::CreateCodecByName(
    const std::string& name) {
  return std::make_shared<AndroidNdkMediaCodec>(name.c_str());
}

std::shared_ptr<Codec> AndroidNdkMediaCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  return std::make_shared<AndroidNdkMediaCodec>(
      mime.c_str(), AndroidNdkMediaCodec::CreatedBy::kCreatedByMime, encoder);
}

}  // namespace media

}  // namespace ave
