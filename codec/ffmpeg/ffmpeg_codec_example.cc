/*
 * ffmpeg_codec_example.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "base/attributes.h"
#include "base/logging.h"
#include "ffmpeg_codec_factory.h"

using ave::media::FFmpegCodecFactory;

int main(int argc AVE_MAYBE_UNUSED, char** argv AVE_MAYBE_UNUSED) {
  FFmpegCodecFactory codec_factory;
  AVE_LOG(LS_INFO) << "FFmpeg Codec factory name: " << codec_factory.name()
                   << ", priority: " << codec_factory.priority();

  auto codec_infos = codec_factory.GetSupportedCodecs();
  for (const auto& codec_info : codec_infos) {
    AVE_LOG(LS_INFO) << "Codec name: " << codec_info.name
                     << ", mime: " << codec_info.mime
                     << ", media type: " << codec_info.media_type
                     << ", is encoder: " << codec_info.is_encoder
                     << ", hardware accelerated: "
                     << codec_info.hardware_accelerated;
  }

  return 0;
}
