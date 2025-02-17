/*
 * media_format.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_FORMAT_H
#define MEDIA_FORMAT_H

#include <cstdint>
#include <variant>

#include "../audio/channel_layout.h"
#include "../codec/codec_id.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media_utils.h"
#include "message.h"

namespace ave {
namespace media {

// Both used for track and sample
class MediaFormat {
 public:
  enum class FormatType {
    kTrack,
    kSample,
  };
  using FormatInfo = std::variant<MediaTrackInfo, MediaSampleInfo>;

  static MediaFormat Create(MediaType stream_type = MediaType::AUDIO,
                            FormatType format_type = FormatType::kSample);
  static std::shared_ptr<MediaFormat> CreatePtr(
      MediaType stream_type = MediaType::AUDIO,
      FormatType format_type = FormatType::kSample);

  explicit MediaFormat(MediaType stream_type = MediaType::AUDIO,
                       FormatType format_type = FormatType::kSample);
  virtual ~MediaFormat() = default;

  MediaTrackInfo& track_info();
  MediaSampleInfo& sample_info();

  /****** 1. track and sample all use ******/
  /****** 1.1 all use ******/
  MediaFormat& SetStreamType(MediaType stream_type);
  MediaType stream_type() const;

  MediaFormat& SetMime(const char* mime);
  const std::string& mime() const;

  MediaFormat& SetName(const char* name);
  const std::string& name() const;

  MediaFormat& SetFullName(const char* name);
  const std::string& full_name() const;

  MediaFormat& SetCodec(CodecId codec);
  CodecId codec() const;

  MediaFormat& SetBitrate(int64_t bps);
  int64_t bitrate() const;

  MediaFormat& SetDuration(base::TimeDelta duration);
  base::TimeDelta duration() const;

  MediaFormat& SetPrivateData(uint32_t size, void* data);
  std::shared_ptr<base::Buffer> private_data();
  // dereference private data if not use any more
  MediaFormat& ClearPrivateData();

  /****** 1.2 audio use ******/
  MediaFormat& SetSampleRate(uint32_t sample_rate_hz);
  uint32_t sample_rate() const;

  MediaFormat& SetChannelLayout(ChannelLayout channel_layout);
  ChannelLayout channel_layout() const;

  MediaFormat& SetSamplesPerChannel(int64_t samples_per_channel);
  int64_t samples_per_channel() const;

  MediaFormat& SetBitsPerSample(int16_t bits_per_sample);
  int16_t bits_per_sample() const;

  /****** 1.3 video use ******/
  MediaFormat& SetWidth(int16_t width);
  int16_t width() const;

  MediaFormat& SetHeight(int16_t height);
  int16_t height() const;

  MediaFormat& SetStride(int16_t stride);
  int16_t stride() const;

  MediaFormat& SetFrameRate(int16_t fps);
  int16_t fps() const;

  MediaFormat& SetPixelFormat(PixelFormat pixel_format);
  PixelFormat pixel_format() const;

  MediaFormat& SetPictureType(PictureType picture_type);
  PictureType picture_type() const;

  MediaFormat& SetRotation(int16_t rotation);
  int16_t rotation() const;

  MediaFormat& SetQp(int16_t qp);
  int16_t qp() const;

  /****** 2. track info ******/
  /****** 2.1 all track same info ******/

  /****** 2.2 audio track specific ******/

  /****** 2.3 video track specific ******/

  /****** 3. sample specific ******/
  /****** 3.1 all sample same info ******/
  MediaFormat& SetPts(base::Timestamp pts);
  base::Timestamp pts() const;

  MediaFormat& SetDts(base::Timestamp dts);
  base::Timestamp dts() const;

  MediaFormat& SetEos(bool eos);
  bool eos() const;

  /****** 3.1 all sample same info ******/

  /****** 3.1 all sample same info ******/

  // meta data
  std::shared_ptr<Message>& meta();

 private:
  FormatType format_type_;

  ave::media::MediaType stream_type_;
  std::string mime_;
  std::string name_;
  std::string full_name_;

  FormatInfo info_;
  std::shared_ptr<Message> meta_;
};

// std::string ToLogString(const MediaFormat& format);

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_FORMAT_H */
