/*
 * video_codec_property.cc
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "video_codec_property.h"

#include "base/checks.h"

namespace ave {
namespace media {

namespace {
// NOLINTBEGIN(modernize-avoid-c-arrays)
constexpr char kPayloadNameVp8[] = "VP8";
constexpr char kPayloadNameVp9[] = "VP9";
constexpr char kPayloadNameAv1[] = "AV1X";
constexpr char kPayloadNameH264[] = "H264";
constexpr char kPayloadNameH265[] = "H265";
constexpr char kPayloadNameGeneric[] = "Generic";
// constexpr char kPayloadNameMultiplex[] = "Multiplex";
// NOLINTEND(modernize-avoid-c-arrays)
}  // namespace

VideoCodecProperty::VideoCodecProperty()
    : codec_id(CodecId::AV_CODEC_ID_NONE),
      mode(VideoCodecMode::kRealtimeVideo),
      width(0),
      height(0),
      bit_rate(0),
      bit_rate_range({0, 0}),
      frame_rate(0),
      qp_range({0, 0}) {}

H264Specific* VideoCodecProperty::H264() {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_H264);
  return std::get_if<H264Specific>(&specific_data_);
}

const H264Specific& VideoCodecProperty::H264() const {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_H264);
  return std::get<H264Specific>(specific_data_);
}

H265Specific* VideoCodecProperty::H265() {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_H265);
  return std::get_if<H265Specific>(&specific_data_);
}

const H265Specific& VideoCodecProperty::H265() const {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_H265);
  return std::get<H265Specific>(specific_data_);
}

VP8Specific* VideoCodecProperty::VP8() {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_VP8);
  return std::get_if<VP8Specific>(&specific_data_);
}

const VP8Specific& VideoCodecProperty::VP8() const {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_VP8);
  return std::get<VP8Specific>(specific_data_);
}

VP9Specific* VideoCodecProperty::VP9() {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_VP9);
  return std::get_if<VP9Specific>(&specific_data_);
}

const VP9Specific& VideoCodecProperty::VP9() const {
  AVE_DCHECK(codec_id == CodecId::AV_CODEC_ID_VP9);
  return std::get<VP9Specific>(specific_data_);
}

const char* CodecName(CodecId type) {
  switch (type) {
    case CodecId::AV_CODEC_ID_VP8:
      return kPayloadNameVp8;
    case CodecId::AV_CODEC_ID_VP9:
      return kPayloadNameVp9;
    case CodecId::AV_CODEC_ID_AV1:
      return kPayloadNameAv1;
    case CodecId::AV_CODEC_ID_H264:
      return kPayloadNameH264;
    case CodecId::AV_CODEC_ID_H265:
      return kPayloadNameH265;
    default:
      return kPayloadNameGeneric;
  }
}

}  // namespace media
}  // namespace ave
