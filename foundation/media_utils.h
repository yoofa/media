/*
 * media_utils.h
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_UTILS_H
#define MEDIA_UTILS_H

#include <sys/types.h>

#include <variant>

#include "base/units/time_delta.h"
#include "base/units/timestamp.h"

#include "../audio/channel_layout.h"
#include "../codec/codec_id.h"
#include "pixel_format.h"

namespace ave {
namespace media {

enum class MediaType {
  UNKNOWN = -1,  ///< Usually treated as MEDIA_TYPE_DATA
  VIDEO,
  AUDIO,
  DATA,  ///< Opaque data information usually continuous
  TIMED_TEXT,
  SUBTITLE,
  ATTACHMENT,  ///< Opaque data information usually sparse
  NB,
  MAX,
};

enum class PictureType {
  NONE = -1,
  I,
  P,
  B,
  S,
  SI,
  SP,
  BI,
  D,
};

const char* get_media_type_string(enum MediaType media_type);

struct AudioSampleInfo {
  CodecId codec_id = CodecId::AVE_CODEC_ID_NONE;
  int64_t sample_rate_hz = -1;
  ChannelLayout channel_layout = CHANNEL_LAYOUT_NONE;
  int64_t samples_per_channel = -1;
  int16_t bits_per_sample = -1;

  base::Timestamp pts = base::Timestamp::Zero();
  base::Timestamp dts = base::Timestamp::Zero();
  base::TimeDelta duration = base::TimeDelta::Zero();

  bool eos = false;
};

struct VideoSampleInfo {
  CodecId codec_id = CodecId::AVE_CODEC_ID_NONE;
  int16_t stride = -1;
  int16_t width = -1;
  int16_t height = -1;
  int16_t rotation = -1;

  // timestamp
  base::Timestamp pts = base::Timestamp::Zero();
  base::Timestamp dts = base::Timestamp::Zero();
  base::TimeDelta duration = base::TimeDelta::Zero();

  bool eos = false;

  // raw
  PixelFormat pixel_format = PixelFormat::AVE_PIX_FMT_NONE;

  // encoded
  PictureType picture_type = PictureType::NONE;
  int16_t qp = -1;
};

using SampleInfo = std::variant<int, AudioSampleInfo, VideoSampleInfo>;

struct AudioTrackInfo {
  CodecId codec_id = CodecId::AVE_CODEC_ID_NONE;
  base::TimeDelta duration = base::TimeDelta::Zero();
  int64_t bitrate_bps = -1;
  int64_t sample_rate_hz = -1;
  ChannelLayout channel_layout = CHANNEL_LAYOUT_NONE;
  int64_t samples_per_channel = -1;
  int16_t bits_per_sample = -1;
};

struct VideoTrackInfo {
  CodecId codec_id = CodecId::AVE_CODEC_ID_NONE;
  base::TimeDelta duration = base::TimeDelta::Zero();
  int64_t bitrate_bps = -1;
  int16_t stride = -1;
  int16_t width = -1;
  int16_t height = -1;
  int16_t rotation = -1;
  PixelFormat pixel_format = PixelFormat::AVE_PIX_FMT_NONE;
};

struct OtherTrackInfo {};

struct MediaTrackInfo {
  explicit MediaTrackInfo(MediaType type) : track_type(type) {
    if (track_type == MediaType::AUDIO) {
      track_info = AudioTrackInfo();
    } else if (track_type == MediaType::VIDEO) {
      track_info = VideoTrackInfo();
    } else {
      track_info = OtherTrackInfo();
    }
  }
  AudioTrackInfo& audio() { return std::get<AudioTrackInfo>(track_info); }
  VideoTrackInfo& video() { return std::get<VideoTrackInfo>(track_info); }

  const AudioTrackInfo& audio() const {
    return std::get<AudioTrackInfo>(track_info);
  }
  const VideoTrackInfo& video() const {
    return std::get<VideoTrackInfo>(track_info);
  }

  MediaType track_type = MediaType::UNKNOWN;
  std::variant<OtherTrackInfo, AudioTrackInfo, VideoTrackInfo> track_info;
};

MediaType CodecMediaType(CodecId codec_id);

}  // namespace media
}  // namespace ave
#endif /* !MEDIA_UTILS_H */
