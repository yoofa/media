/*
 * dummy_codec_factory.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "dummy_codec_factory.h"
#include "dummy_codec.h"

namespace ave {
namespace media {
namespace test {

DummyCodecFactory::DummyCodecFactory() {
  // Add supported codecs
  CodecInfo info;
  info.name = "dummy";
  info.mime = "video/dummy";
  info.media_type = MediaType::VIDEO;
  info.is_encoder = false;
  info.hardware_accelerated = false;
  supported_codecs_.push_back(info);

  info.is_encoder = true;
  supported_codecs_.push_back(info);

  info.mime = "audio/dummy";
  info.media_type = MediaType::AUDIO;
  info.is_encoder = false;
  supported_codecs_.push_back(info);

  info.is_encoder = true;
  supported_codecs_.push_back(info);
}

DummyCodecFactory::~DummyCodecFactory() = default;

std::vector<CodecInfo> DummyCodecFactory::GetSupportedCodecs() {
  return supported_codecs_;
}

std::shared_ptr<Codec> DummyCodecFactory::CreateCodecByType(CodecId codec_id,
                                                            bool encoder) {
  return std::make_shared<DummyCodec>(encoder);
}

std::shared_ptr<Codec> DummyCodecFactory::CreateCodecByName(
    const std::string& name) {
  if (name == "dummy_encoder") {
    return std::make_shared<DummyCodec>(true);
  }
  if (name == "dummy_decoder") {
    return std::make_shared<DummyCodec>(false);
  }
  return nullptr;
}

std::shared_ptr<Codec> DummyCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  if (mime == "video/dummy" || mime == "audio/dummy") {
    return std::make_shared<DummyCodec>(encoder);
  }
  return nullptr;
}

}  // namespace test
}  // namespace media
}  // namespace ave
