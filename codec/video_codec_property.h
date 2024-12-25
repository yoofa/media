/*
 * video_codec_property.h
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef VIDEO_CODEC_PROPERTY_H
#define VIDEO_CODEC_PROPERTY_H

#include <stdint.h>

#include <variant>

#include "codec_id.h"

namespace ave {
namespace media {

// Video codec
enum class VideoCodecComplexity {
  kComplexityNormal = 0,
  kComplexityHigh = 1,
  kComplexityHigher = 2,
  kComplexityMax = 3
};

// h264 specific
struct H264Specific {
  bool frame_dropping_on;
  int key_frame_interval;
  uint8_t number_of_temporal_layers;
};

// h265 specific
struct H265Specific {
  bool frame_dropping_on;
  int key_frame_interval;
  uint8_t number_of_temporal_layers;
};

// vp8 specific
struct VP8Specific {
  VideoCodecComplexity complexity;
  bool frame_dropping_on;
  int key_frame_interval;
  uint8_t number_of_temporal_layers;
  bool denoising_on;
  bool automatic_resize_on;
};

enum class InterLayerPredMode : int {
  kOff = 0,      // Inter-layer prediction is disabled.
  kOn = 1,       // Inter-layer prediction is enabled.
  kOnKeyPic = 2  // Inter-layer prediction is enabled but limited to key frames.
};

// vp9 specific
struct VP9Specific {
  VideoCodecComplexity complexity;
  bool frame_dropping_on;
  int key_frame_interval;
  uint8_t number_of_temporal_layers;
  uint8_t number_of_spatial_layers;
  bool denoising_on;
  bool automatic_resize_on;
  bool adaptive_qp_mode;
  bool flexible_mode;
  InterLayerPredMode inter_layer_pred;
};

using CodecSpecific = std::variant<
    //clang-format off
    H264Specific,
    H265Specific,
    VP8Specific,
    VP9Specific
    // clang-format on
    >;

enum class VideoCodecMode { kRealtimeVideo, kScreensharing };

class VideoCodecProperty {
 public:
  VideoCodecProperty();

  CodecId codec_id;

  VideoCodecMode mode;

  size_t width;
  size_t height;

  // bit rate in kbps
  uint32_t bit_rate;
  struct BitRateRange {
    uint32_t min;
    uint32_t max;
  } bit_rate_range;

  uint16_t frame_rate;

  struct QpRange {
    uint16_t min;
    uint16_t max;
  } qp_range;

  H264Specific* H264();
  const H264Specific& H264() const;
  H265Specific* H265();
  const H265Specific& H265() const;
  VP8Specific* VP8();
  const VP8Specific& VP8() const;
  VP9Specific* VP9();
  const VP9Specific& VP9() const;

 private:
  CodecSpecific specific_data_;
};

const char* CodecName(CodecId type);

}  // namespace media
}  // namespace ave

#endif /* !VIDEO_CODEC_PROPERTY_H */
