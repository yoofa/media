/*
 * media_meta.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_META_H
#define MEDIA_META_H

#include <cstdint>
#include <variant>

#include "../audio/channel_layout.h"
#include "../codec/codec_id.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/foundation/message_object.h"
#include "media_utils.h"
#include "message.h"

namespace ave {
namespace media {

// Both used for track and sample
class MediaMeta : public MessageObject {
 public:
  enum class FormatType {
    kTrack,
    kSample,
  };
  using FormatInfo = std::variant<MediaTrackInfo, MediaSampleInfo>;

  static MediaMeta Create(MediaType stream_type = MediaType::AUDIO,
                          FormatType format_type = FormatType::kSample);
  static std::shared_ptr<MediaMeta> CreatePtr(
      MediaType stream_type = MediaType::AUDIO,
      FormatType format_type = FormatType::kSample);

  explicit MediaMeta(MediaType stream_type = MediaType::AUDIO,
                     FormatType format_type = FormatType::kSample);
  ~MediaMeta() override = default;

  MediaTrackInfo& track_info();
  MediaSampleInfo& sample_info();

  /****** 1. track and sample all use ******/
  /****** 1.1 all use ******/
  MediaMeta& SetStreamType(MediaType stream_type);
  MediaType stream_type() const;

  MediaMeta& SetMime(const char* mime);
  const std::string& mime() const;

  MediaMeta& SetName(const char* name);
  const std::string& name() const;

  MediaMeta& SetFullName(const char* name);
  const std::string& full_name() const;

  MediaMeta& SetCodec(CodecId codec);
  CodecId codec() const;

  MediaMeta& SetBitrate(int64_t bps);
  int64_t bitrate() const;

  MediaMeta& SetDuration(base::TimeDelta duration);
  base::TimeDelta duration() const;

  MediaMeta& SetPrivateData(uint32_t size, void* data);
  std::shared_ptr<base::Buffer> private_data();
  // dereference private data if not use any more
  MediaMeta& ClearPrivateData();

  /****** 1.2 audio use ******/
  MediaMeta& SetSampleRate(uint32_t sample_rate_hz);
  uint32_t sample_rate() const;

  MediaMeta& SetChannelLayout(ChannelLayout channel_layout);
  ChannelLayout channel_layout() const;

  MediaMeta& SetSamplesPerChannel(int64_t samples_per_channel);
  int64_t samples_per_channel() const;

  MediaMeta& SetBitsPerSample(int16_t bits_per_sample);
  int16_t bits_per_sample() const;

  /****** 1.3 video use ******/
  MediaMeta& SetWidth(int32_t width);
  int32_t width() const;

  MediaMeta& SetHeight(int32_t height);
  int32_t height() const;

  MediaMeta& SetStride(int32_t stride);
  int32_t stride() const;

  MediaMeta& SetFrameRate(int32_t fps);
  int32_t fps() const;

  MediaMeta& SetPixelFormat(PixelFormat pixel_format);
  PixelFormat pixel_format() const;

  MediaMeta& SetPictureType(PictureType picture_type);
  PictureType picture_type() const;

  MediaMeta& SetRotation(int16_t rotation);
  int16_t rotation() const;

  MediaMeta& SetQp(int16_t qp);
  int16_t qp() const;

  MediaMeta& SetColorPrimaries(ColorPrimaries color_primaries);
  ColorPrimaries color_primaries() const;

  MediaMeta& SetColorTransfer(ColorTransfer color_transfer);
  ColorTransfer color_transfer() const;

  MediaMeta& SetColorSpace(ColorSpace color_space);
  ColorSpace color_space() const;

  MediaMeta& SetColorRange(ColorRange color_range);
  ColorRange color_range() const;

  MediaMeta& SetFieldOrder(FieldOrder field_order);
  FieldOrder field_order() const;

  MediaMeta& SetSampleAspectRatio(std::pair<int16_t, int16_t> sar);
  std::pair<int16_t, int16_t> sample_aspect_ratio() const;

  /****** 2. track info ******/
  /****** 2.1 all track same info ******/

  /****** 2.2 audio track specific ******/

  /****** 2.3 video track specific ******/
  MediaMeta& SetCodecProfile(int32_t profile);
  int32_t codec_profile() const;

  MediaMeta& SetCodecLevel(int32_t level);
  int32_t codec_level() const;

  // TODO(youfa): really need this?
  MediaMeta& SetTimeBase(std::pair<int32_t, int32_t> time_base);
  std::pair<int32_t, int32_t> time_base() const;

  /****** 3. sample specific ******/
  /****** 3.1 all sample same info ******/
  MediaMeta& SetPts(base::Timestamp pts);
  base::Timestamp pts() const;

  MediaMeta& SetDts(base::Timestamp dts);
  base::Timestamp dts() const;

  MediaMeta& SetEos(bool eos);
  bool eos() const;

  /****** 3.2 audio sample same info ******/

  /****** 3.3 video sample same info ******/

  // meta data
  std::shared_ptr<Message>& ext_msg();

 private:
  FormatType format_type_;

  ave::media::MediaType stream_type_;
  std::string mime_;
  std::string name_;
  std::string full_name_;

  FormatInfo info_;
  std::shared_ptr<Message> ext_msg_;
};

// std::string ToLogString(const MediaMeta& format);

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_META_H */
