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
#ifdef AVE_FFMPEG_CODEC
  ffmpeg_factory_ = std::make_shared<FFmpegCodecFactory>();
#endif
}

SoftwareCodecFactory::~SoftwareCodecFactory() {}

std::vector<CodecInfo> SoftwareCodecFactory::GetSupportedCodecs() {
  std::vector<CodecInfo> codecs;
#ifdef AVE_FFMPEG_CODEC
  if (ffmpeg_factory_) {
    codecs = ffmpeg_factory_->GetSupportedCodecs();
  }
#endif
  return codecs;
}

std::shared_ptr<Codec> SoftwareCodecFactory::CreateCodecByType(CodecId codec_id,
                                                               bool encoder) {
  if (codec_id == CodecId::AVE_CODEC_ID_OPUS) {
    return std::make_shared<OpusCodec>(encoder);
  }
  // TODO: Add logic to choose between FFmpeg and other software codecs (e.g.
  // FDK-AAC)
#ifdef AVE_FFMPEG_CODEC
  if (ffmpeg_factory_) {
    return ffmpeg_factory_->CreateCodecByType(codec_id, encoder);
  }
#endif
  return nullptr;
}

std::shared_ptr<Codec> SoftwareCodecFactory::CreateCodecByName(
    const std::string& name) {
#ifdef AVE_FFMPEG_CODEC
  if (ffmpeg_factory_) {
    return ffmpeg_factory_->CreateCodecByName(name);
  }
#endif
  return nullptr;
}

std::shared_ptr<Codec> SoftwareCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
#ifdef AVE_FFMPEG_CODEC
  if (ffmpeg_factory_) {
    return ffmpeg_factory_->CreateCodecByMime(mime, encoder);
  }
#endif
  return nullptr;
}

}  // namespace media
}  // namespace ave
