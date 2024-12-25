/*
 * media_utils.cc
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_utils.h"

namespace ave {
namespace media {

const char* get_media_type_string(MediaType media_type) {
  switch (media_type) {
    case MediaType::VIDEO:
      return "video";
    case MediaType::AUDIO:
      return "audio";
    case MediaType::DATA:
      return "data";
    case MediaType::SUBTITLE:
      return "subtitle";
    case MediaType::ATTACHMENT:
      return "attachment";
    default:
      return nullptr;
  }
}

MediaType CodecMediaType(CodecId codec_id) {
  if (codec_id >= CodecId::AVE_CODEC_ID_FIRST_VIDEO &&
      codec_id <= CodecId::AVE_CODEC_ID_LAST_VIDEO) {
    return MediaType::VIDEO;
  }
  if (codec_id >= CodecId::AVE_CODEC_ID_FIRST_AUDIO &&
      codec_id <= CodecId::AVE_CODEC_ID_LAST_AUDIO) {
    return MediaType::AUDIO;
  }

  if (codec_id >= CodecId::AVE_CODEC_ID_FIRST_SUBTITLE &&
      codec_id <= CodecId::AVE_CODEC_ID_LAST_SUBTITLE) {
    return MediaType::SUBTITLE;
  }

  return MediaType::UNKNOWN;
}

}  // namespace media
}  // namespace ave
