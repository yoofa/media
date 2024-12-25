/*
 * media_format.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media_format.h"
#include "base/logging.h"
#include "media_utils.h"

namespace ave {
namespace media {

namespace {
MediaFormat::FormatInfo CreateFormatInfo(MediaFormat::FormatType format_type,
                                         MediaType stream_type) {
  if (format_type == MediaFormat::FormatType::kTrack) {
    return MediaTrackInfo(stream_type);
  }
  return MediaSampleInfo(stream_type);
}
}  // namespace

MediaFormat MediaFormat::Create(MediaType stream_type, FormatType format_type) {
  return MediaFormat(stream_type, format_type);
}

std::shared_ptr<MediaFormat> MediaFormat::CreatePtr(MediaType stream_type,
                                                    FormatType format_type) {
  return std::make_shared<MediaFormat>(stream_type, format_type);
}

MediaFormat::MediaFormat(MediaType stream_type, FormatType format_type)
    : format_type_(format_type),
      stream_type_(stream_type),
      info_(CreateFormatInfo(format_type, stream_type)),
      meta_(nullptr) {}

MediaTrackInfo& MediaFormat::track_info() {
  if (format_type_ != FormatType::kTrack) {
    AVE_LOG(LS_ERROR) << "Accessing track info on sample format";
  }
  return std::get<MediaTrackInfo>(info_);
}

MediaSampleInfo& MediaFormat::sample_info() {
  if (format_type_ != FormatType::kSample) {
    AVE_LOG(LS_ERROR) << "Accessing sample info on track format";
  }
  return std::get<MediaSampleInfo>(info_);
}

MediaFormat& MediaFormat::SetStreamType(MediaType stream_type) {
  if (stream_type_ != stream_type) {
    stream_type_ = stream_type;
    if (format_type_ == FormatType::kTrack) {
      info_ = MediaTrackInfo(stream_type_);
    } else {
      info_ = MediaSampleInfo(stream_type_);
    }
  }
  return *this;
}

MediaType MediaFormat::stream_type() const {
  return stream_type_;
}

MediaFormat& MediaFormat::SetMime(const char* mime) {
  if (!mime) {
    AVE_LOG(LS_WARNING) << "SetMime failed, mime is null";
  } else {
    mime_ = mime;
  }
  return *this;
}

const std::string& MediaFormat::mime() const {
  return mime_;
}

MediaFormat& MediaFormat::SetName(const char* name) {
  if (!name) {
    AVE_LOG(LS_WARNING) << "SetName failed, name is null";
  } else {
    name_ = name;
  }
  return *this;
}

const std::string& MediaFormat::name() const {
  return name_;
}

MediaFormat& MediaFormat::SetFullName(const char* name) {
  if (!name) {
    AVE_LOG(LS_WARNING) << "SetFullName failed, name is null";
  } else {
    full_name_ = name;
  }
  return *this;
}

const std::string& MediaFormat::full_name() const {
  return full_name_;
}

MediaFormat& MediaFormat::SetCodec(CodecId codec) {
  if (format_type_ == FormatType::kTrack) {
    auto& track = track_info();
    switch (stream_type_) {
      case MediaType::VIDEO:
        track.video().codec_id = codec;
        break;
      case MediaType::AUDIO:
        track.audio().codec_id = codec;
        break;
      default:
        break;
    }
  } else {
    auto& sample = sample_info();
    switch (stream_type_) {
      case MediaType::VIDEO:
        sample.video().codec_id = codec;
        break;
      case MediaType::AUDIO:
        sample.audio().codec_id = codec;
        break;
      default:
        break;
    }
  }
  return *this;
}

CodecId MediaFormat::codec() const {
  if (format_type_ == FormatType::kTrack) {
    const auto& track = std::get<MediaTrackInfo>(info_);
    switch (stream_type_) {
      case MediaType::VIDEO:
        return track.video().codec_id;
      case MediaType::AUDIO:
        return track.audio().codec_id;
      default:
        return CodecId::AVE_CODEC_ID_NONE;
    }
  } else {
    const auto& sample = std::get<MediaSampleInfo>(info_);
    switch (stream_type_) {
      case MediaType::VIDEO:
        return sample.video().codec_id;
      case MediaType::AUDIO:
        return sample.audio().codec_id;
      default:
        return CodecId::AVE_CODEC_ID_NONE;
    }
  }
}

MediaFormat& MediaFormat::SetBitrate(int64_t bps) {
  if (format_type_ != FormatType::kTrack) {
    AVE_LOG(LS_WARNING) << "SetBitrate only available for track format";
    return *this;
  }

  auto& track = track_info();
  switch (stream_type_) {
    case MediaType::VIDEO:
      track.video().bitrate_bps = bps;
      break;
    case MediaType::AUDIO:
      track.audio().bitrate_bps = bps;
      break;
    default:
      break;
  }
  return *this;
}

int64_t MediaFormat::bitrate() const {
  if (format_type_ != FormatType::kTrack) {
    AVE_LOG(LS_WARNING) << "bitrate only available for track format";
    return -1;
  }

  const auto& track = std::get<MediaTrackInfo>(info_);
  switch (stream_type_) {
    case MediaType::VIDEO:
      return track.video().bitrate_bps;
    case MediaType::AUDIO:
      return track.audio().bitrate_bps;
    default:
      return -1;
  }
}

MediaFormat& MediaFormat::SetDuration(base::TimeDelta duration) {
  if (format_type_ == FormatType::kTrack) {
    auto& track = track_info();
    switch (stream_type_) {
      case MediaType::VIDEO:
        track.video().duration = duration;
        break;
      case MediaType::AUDIO:
        track.audio().duration = duration;
        break;
      default:
        break;
    }
  } else {
    auto& sample = sample_info();
    switch (stream_type_) {
      case MediaType::VIDEO:
        sample.video().duration = duration;
        break;
      case MediaType::AUDIO:
        sample.audio().duration = duration;
        break;
      default:
        break;
    }
  }
  return *this;
}

base::TimeDelta MediaFormat::duration() const {
  if (format_type_ == FormatType::kTrack) {
    const auto& track = std::get<MediaTrackInfo>(info_);
    switch (stream_type_) {
      case MediaType::VIDEO:
        return track.video().duration;
      case MediaType::AUDIO:
        return track.audio().duration;
      default:
        return base::TimeDelta::Zero();
    }
  } else {
    const auto& sample = std::get<MediaSampleInfo>(info_);
    switch (stream_type_) {
      case MediaType::VIDEO:
        return sample.video().duration;
      case MediaType::AUDIO:
        return sample.audio().duration;
      default:
        return base::TimeDelta::Zero();
    }
  }
}

// media_format.cc

MediaFormat& MediaFormat::SetPrivateData(uint32_t size, void* data) {
  if (data == nullptr) {
    AVE_LOG(LS_WARNING) << "SetPrivateData failed, data is null";
    return *this;
  }
  auto buffer =
      std::make_shared<base::Buffer>(static_cast<uint8_t*>(data), size);
  if (format_type_ == FormatType::kTrack) {
    switch (stream_type_) {
      case MediaType::VIDEO:
        track_info().video().private_data = buffer;
        break;
      case MediaType::AUDIO:
        track_info().audio().private_data = buffer;
        break;
      default:
        break;
    }
  } else {
    switch (stream_type_) {
      case MediaType::VIDEO:
        sample_info().video().private_data = buffer;
        break;
      case MediaType::AUDIO:
        sample_info().audio().private_data = buffer;
        break;
      default:
        break;
    }
  }

  return *this;
}

std::shared_ptr<base::Buffer> MediaFormat::private_data() {
  if (format_type_ == FormatType::kTrack) {
    switch (stream_type_) {
      case MediaType::VIDEO:
        return track_info().video().private_data;
      case MediaType::AUDIO:
        return track_info().audio().private_data;
      default:
        break;
    }
  } else {
    switch (stream_type_) {
      case MediaType::VIDEO:
        return sample_info().video().private_data;
      case MediaType::AUDIO:
        return sample_info().audio().private_data;
      default:
        break;
    }
  }
  return nullptr;
}

MediaFormat& MediaFormat::ClearPrivateData() {
  if (format_type_ == FormatType::kTrack) {
    switch (stream_type_) {
      case MediaType::VIDEO:
        track_info().video().private_data = nullptr;
        break;
      case MediaType::AUDIO:
        track_info().audio().private_data = nullptr;
        break;
      default:
        break;
    }
  } else {
    switch (stream_type_) {
      case MediaType::VIDEO:
        sample_info().video().private_data = nullptr;
        break;
      case MediaType::AUDIO:
        sample_info().audio().private_data = nullptr;
        break;
      default:
        break;
    }
  }
  return *this;
}

// Video specific methods
MediaFormat& MediaFormat::SetWidth(int16_t width) {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "SetWidth failed, stream type is not video";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().video().width = width;
  } else {
    sample_info().video().width = width;
  }
  return *this;
}

int16_t MediaFormat::width() const {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "width failed, stream type is not video";
    return -1;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).video().width;
  }
  return std::get<MediaSampleInfo>(info_).video().width;
}

MediaFormat& MediaFormat::SetHeight(int16_t height) {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "SetHeight failed, stream type is not video";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().video().height = height;
  } else {
    sample_info().video().height = height;
  }
  return *this;
}

int16_t MediaFormat::height() const {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "height failed, stream type is not video";
    return -1;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).video().height;
  }
  return std::get<MediaSampleInfo>(info_).video().height;
}

MediaFormat& MediaFormat::SetStride(int16_t stride) {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "SetStride failed, stream type is not video";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().video().stride = stride;
  } else {
    sample_info().video().stride = stride;
  }
  return *this;
}

int16_t MediaFormat::stride() const {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "stride failed, stream type is not video";
    return -1;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).video().stride;
  }
  return std::get<MediaSampleInfo>(info_).video().stride;
}

MediaFormat& MediaFormat::SetFrameRate(int16_t fps) {
  if (stream_type_ != MediaType::VIDEO || format_type_ != FormatType::kTrack) {
    AVE_LOG(LS_WARNING) << "SetFrameRate failed, invalid format";
    return *this;
  }
  track_info().video().fps = fps;
  return *this;
}

int16_t MediaFormat::fps() const {
  if (stream_type_ != MediaType::VIDEO || format_type_ != FormatType::kTrack) {
    AVE_LOG(LS_WARNING) << "fps failed, invalid format";
    return -1;
  }
  return std::get<MediaTrackInfo>(info_).video().fps;
}

MediaFormat& MediaFormat::SetPixelFormat(PixelFormat pixel_format) {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "SetPixelFormat failed, stream type is not video";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().video().pixel_format = pixel_format;
  }
  return *this;
}

PixelFormat MediaFormat::pixel_format() const {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "pixel_format failed, stream type is not video";
    return PixelFormat::AVE_PIX_FMT_NONE;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).video().pixel_format;
  }
  return PixelFormat::AVE_PIX_FMT_NONE;
}

MediaFormat& MediaFormat::SetPictureType(PictureType picture_type) {
  if (stream_type_ != MediaType::VIDEO || format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "SetPictureType failed, invalid format";
    return *this;
  }
  sample_info().video().picture_type = picture_type;
  return *this;
}

PictureType MediaFormat::picture_type() const {
  if (stream_type_ != MediaType::VIDEO || format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "picture_type failed, invalid format";
    return PictureType::NONE;
  }
  return std::get<MediaSampleInfo>(info_).video().picture_type;
}

MediaFormat& MediaFormat::SetRotation(int16_t rotation) {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "SetRotation failed, stream type is not video";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().video().rotation = rotation;
  } else {
    sample_info().video().rotation = rotation;
  }
  return *this;
}

int16_t MediaFormat::rotation() const {
  if (stream_type_ != MediaType::VIDEO) {
    AVE_LOG(LS_WARNING) << "rotation failed, stream type is not video";
    return -1;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).video().rotation;
  }
  return std::get<MediaSampleInfo>(info_).video().rotation;
}

MediaFormat& MediaFormat::SetQp(int16_t qp) {
  if (stream_type_ != MediaType::VIDEO || format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "SetQp failed, invalid format";
    return *this;
  }
  sample_info().video().qp = qp;
  return *this;
}

int16_t MediaFormat::qp() const {
  if (stream_type_ != MediaType::VIDEO || format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "qp failed, invalid format";
    return -1;
  }
  return std::get<MediaSampleInfo>(info_).video().qp;
}

// Audio specific methods
MediaFormat& MediaFormat::SetSampleRate(uint32_t sample_rate_hz) {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING) << "SetSampleRate failed, stream type is not audio";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().audio().sample_rate_hz = sample_rate_hz;
  }
  return *this;
}

uint32_t MediaFormat::sample_rate() const {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING) << "sample_rate failed, stream type is not audio";
    return 0;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).audio().sample_rate_hz;
  }
  return 0;
}

MediaFormat& MediaFormat::SetChannelLayout(ChannelLayout channel_layout) {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING) << "SetChannelLayout failed, stream type is not audio";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().audio().channel_layout = channel_layout;
  }
  return *this;
}

ChannelLayout MediaFormat::channel_layout() const {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING) << "channel_layout failed, stream type is not audio";
    return CHANNEL_LAYOUT_NONE;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).audio().channel_layout;
  }
  return CHANNEL_LAYOUT_NONE;
}

MediaFormat& MediaFormat::SetSamplesPerChannel(int64_t samples_per_channel) {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING)
        << "SetSamplesPerChannel failed, stream type is not audio";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().audio().samples_per_channel = samples_per_channel;
  }
  return *this;
}

int64_t MediaFormat::samples_per_channel() const {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING)
        << "samples_per_channel failed, stream type is not audio";
    return -1;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).audio().samples_per_channel;
  }
  return -1;
}

MediaFormat& MediaFormat::SetBitsPerSample(int16_t bits_per_sample) {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING) << "SetBitsPerSample failed, stream type is not audio";
    return *this;
  }

  if (format_type_ == FormatType::kTrack) {
    track_info().audio().bits_per_sample = bits_per_sample;
  }
  return *this;
}

int16_t MediaFormat::bits_per_sample() const {
  if (stream_type_ != MediaType::AUDIO) {
    AVE_LOG(LS_WARNING) << "bits_per_sample failed, stream type is not audio";
    return -1;
  }

  if (format_type_ == FormatType::kTrack) {
    return std::get<MediaTrackInfo>(info_).audio().bits_per_sample;
  }
  return -1;
}

// Sample specific methods
MediaFormat& MediaFormat::SetPts(base::Timestamp pts) {
  if (format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "SetPts failed, not a sample format";
    return *this;
  }

  auto& sample = sample_info();
  switch (stream_type_) {
    case MediaType::VIDEO:
      sample.video().pts = pts;
      break;
    case MediaType::AUDIO:
      sample.audio().pts = pts;
      break;
    default:
      break;
  }
  return *this;
}

base::Timestamp MediaFormat::pts() const {
  if (format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "pts failed, not a sample format";
    return base::Timestamp::Zero();
  }

  const auto& sample = std::get<MediaSampleInfo>(info_);
  switch (stream_type_) {
    case MediaType::VIDEO:
      return sample.video().pts;
    case MediaType::AUDIO:
      return sample.audio().pts;
    default:
      return base::Timestamp::Zero();
  }
}

MediaFormat& MediaFormat::SetDts(base::Timestamp dts) {
  if (format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "SetDts failed, not a sample format";
    return *this;
  }

  auto& sample = sample_info();
  switch (stream_type_) {
    case MediaType::VIDEO:
      sample.video().dts = dts;
      break;
    case MediaType::AUDIO:
      sample.audio().dts = dts;
      break;
    default:
      break;
  }
  return *this;
}

base::Timestamp MediaFormat::dts() const {
  if (format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "dts failed, not a sample format";
    return base::Timestamp::Zero();
  }

  const auto& sample = std::get<MediaSampleInfo>(info_);
  switch (stream_type_) {
    case MediaType::VIDEO:
      return sample.video().dts;
    case MediaType::AUDIO:
      return sample.audio().dts;
    default:
      return base::Timestamp::Zero();
  }
}

MediaFormat& MediaFormat::SetEos(bool eos) {
  if (format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "SetEos failed, not a sample format";
    return *this;
  }

  auto& sample = sample_info();
  switch (stream_type_) {
    case MediaType::VIDEO:
      sample.video().eos = eos;
      break;
    case MediaType::AUDIO:
      sample.audio().eos = eos;
      break;
    default:
      break;
  }
  return *this;
}

bool MediaFormat::eos() const {
  if (format_type_ != FormatType::kSample) {
    AVE_LOG(LS_WARNING) << "eos failed, not a sample format";
    return false;
  }

  const auto& sample = std::get<MediaSampleInfo>(info_);
  switch (stream_type_) {
    case MediaType::VIDEO:
      return sample.video().eos;
    case MediaType::AUDIO:
      return sample.audio().eos;
    default:
      return false;
  }
}

std::shared_ptr<Message>& MediaFormat::meta() {
  if (!meta_) {
    meta_ = std::make_shared<Message>();
  }
  return meta_;
}

}  // namespace media
}  // namespace ave
