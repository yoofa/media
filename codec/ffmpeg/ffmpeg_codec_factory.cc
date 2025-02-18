/*
 * ffmpeg_codec_factory.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "base/logging.h"

#include "ffmpeg_codec.h"
#include "ffmpeg_codec_factory.h"
#include "ffmpeg_codec_utils.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace ave {
namespace media {

FFmpegCodecFactory::FFmpegCodecFactory() = default;
FFmpegCodecFactory::~FFmpegCodecFactory() = default;

std::shared_ptr<Codec> FFmpegCodecFactory::CreateCodecByType(CodecId codec_id,
                                                             bool encoder) {
  AVCodecID ffmpeg_codec_id = ConvertToFFmpegCodecId(codec_id);
  if (ffmpeg_codec_id == AV_CODEC_ID_NONE) {
    return nullptr;
  }

  const AVCodec* codec = encoder ? avcodec_find_encoder(ffmpeg_codec_id)
                                 : avcodec_find_decoder(ffmpeg_codec_id);
  if (!codec) {
    return nullptr;
  }

  return std::make_shared<FFmpegCodec>(codec, encoder);
}

std::shared_ptr<Codec> FFmpegCodecFactory::CreateCodecByName(
    const std::string& name) {
  const AVCodec* codec = avcodec_find_decoder_by_name(name.c_str());
  if (!codec) {
    codec = avcodec_find_encoder_by_name(name.c_str());
  }
  if (!codec) {
    return nullptr;
  }

  return std::make_shared<FFmpegCodec>(codec, av_codec_is_encoder(codec));
}

std::shared_ptr<Codec> FFmpegCodecFactory::CreateCodecByMime(
    const std::string& mime,
    bool encoder) {
  // TODO: Implement this
  return nullptr;
}

std::vector<CodecInfo> FFmpegCodecFactory::GetSupportedCodecs() {
  if (!supported_codecs_.empty()) {
    return supported_codecs_;
  }

  void* iter = nullptr;
  const AVCodec* codec = nullptr;
  while ((codec = av_codec_iterate(&iter))) {
    CodecInfo info;
    info.name = codec->name;
    // FIXME:
    // FFmpeg doesn't have a direct MIME type, using long_name instead
    info.mime = codec->long_name;  // Note:

    // Determine media type
    if (codec->type == AVMEDIA_TYPE_VIDEO) {
      info.media_type = MediaType::VIDEO;
    } else if (codec->type == AVMEDIA_TYPE_AUDIO) {
      info.media_type = MediaType::AUDIO;
    } else {
      continue;  // Skip other types
    }

    info.is_encoder = av_codec_is_encoder(codec);
    info.hardware_accelerated =
        (codec->capabilities & AV_CODEC_CAP_HARDWARE) != 0;

    // Add logic to retrieve additional information
    info.max_resolution = {codec->max_lowres, codec->max_lowres};
    info.max_bitrate = codec->bit_rate;
    info.profile = codec->profile;
    info.level = codec->level;
    info.supported_color_formats = {codec->pix_fmts};
    info.frame_rate_range = {codec->framerate.num, codec->framerate.den};
    info.supported_sample_rates = {codec->supported_samplerates};
    info.supported_channel_layouts = {codec->channel_layouts};
    info.latency = codec->delay;

    supported_codecs_.push_back(info);
  }

  return supported_codecs_;
}

}  // namespace media
}  // namespace ave
