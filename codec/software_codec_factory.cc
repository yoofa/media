/*
 * software_codec_factory.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "software_codec_factory.h"

#include "base/logging.h"
#include "opus/opus_codec.h"

namespace ave {
namespace media {

SoftwareCodecFactory::SoftwareCodecFactory() {
  ffmpeg_factory_ = std::make_shared<FFmpegCodecFactory>();
}

SoftwareCodecFactory::~SoftwareCodecFactory() {}

std::vector<CodecInfo> SoftwareCodecFactory::GetSupportedCodecs() {
  // Combine supported codecs from all internal factories
  // Currently only FFmpeg
  return ffmpeg_factory_->GetSupportedCodecs();
}

std::shared_ptr<Codec> SoftwareCodecFactory::CreateCodecByType(CodecId codec_id,
                                                               bool encoder) {
  if (codec_id == CodecId::AVE_CODEC_ID_OPUS) {
    return std::make_shared<OpusCodec>(encoder);
  }
  // TODO: Add logic to choose between FFmpeg and other software codecs (e.g.
  // FDK-AAC)
  return ffmpeg_factory_->CreateCodecByType(codec_id, encoder);
}

std::shared_ptr<Codec> SoftwareCodecFactory::CreateCodecByName(
    const std::string& name) {
  return ffmpeg_factory_->CreateCodecByName(name);
}

std::shared_ptr<Codec> SoftwareCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  return ffmpeg_factory_->CreateCodecByMime(mime, encoder);
}

}  // namespace media
}  // namespace ave
