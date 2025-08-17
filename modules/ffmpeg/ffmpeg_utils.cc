/*
 * ffmpeg_codec_utils.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "ffmpeg_utils.h"

#include "base/attributes.h"
#include "base/checks.h"
#include "base/logging.h"

#include "../../audio/channel_layout.h"
#include "../../codec/codec_id.h"
#include "../../foundation/media_utils.h"

#include "libavutil/channel_layout.h"
#include "third_party/ffmpeg/libavcodec/codec_id.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace ave {
namespace media {
namespace ffmpeg_utils {

static const AVRational kMicrosBase = {1, 1000000};

int64_t ConvertFromTimeBase(const AVRational& time_base, int64_t pkt_pts) {
  return av_rescale_q(pkt_pts, time_base, kMicrosBase);
}

int64_t ConvertToTimeBase(const AVRational& time_base, const int64_t time_us) {
  return av_rescale_q(time_us, kMicrosBase, time_base);
}

// TODO(youfa): print real line number and file name
void ffmpeg_log_default(void* p_unused AVE_MAYBE_UNUSED,
                        int i_level AVE_MAYBE_UNUSED,
                        const char* psz_fmt,
                        va_list arg) {
  std::array<char, 4096> buf{};
  int length = vsnprintf(buf.data(), sizeof(buf), psz_fmt, arg);
  if (length) {
    AVE_LOG(LS_ERROR) << "ffmpeg log:" << buf.data();
  }
}

CodecId ConvertToAVECodecId(AVCodecID ffmpeg_codec_id) {
  switch (ffmpeg_codec_id) {
    /* video codecs */
    case AV_CODEC_ID_H264: {
      return CodecId::AVE_CODEC_ID_H264;
    };
    case AV_CODEC_ID_MPEG4: {
      return CodecId::AVE_CODEC_ID_MPEG4;
    };
    case AV_CODEC_ID_MPEG2VIDEO: {
      return CodecId::AVE_CODEC_ID_MPEG2VIDEO;
    };
    case AV_CODEC_ID_VP8: {
      return CodecId::AVE_CODEC_ID_VP8;
    };
    case AV_CODEC_ID_VP9: {
      return CodecId::AVE_CODEC_ID_VP9;
    };
    case AV_CODEC_ID_HEVC: {
      return CodecId::AVE_CODEC_ID_HEVC;
    };
    case AV_CODEC_ID_AV1: {
      return CodecId::AVE_CODEC_ID_AV1;
    };

    /* audio codecs */
    case AV_CODEC_ID_PCM_S16LE: {
      return CodecId::AVE_CODEC_ID_PCM_S16LE;
    }
    case AV_CODEC_ID_PCM_S16BE: {
      return CodecId::AVE_CODEC_ID_PCM_S16BE;
    }
    case AV_CODEC_ID_MP3: {
      return CodecId::AVE_CODEC_ID_MP3;
    }
    case AV_CODEC_ID_AAC: {
      return CodecId::AVE_CODEC_ID_AAC;
    }
    case AV_CODEC_ID_AC3: {
      return CodecId::AVE_CODEC_ID_AC3;
    }
    case AV_CODEC_ID_EAC3: {
      return CodecId::AVE_CODEC_ID_EAC3;
    }

      /* subtitle codecs */

    default: {
      return CodecId::AVE_CODEC_ID_NONE;
    }
  }
}

AVCodecID ConvertToFFmpegCodecId(CodecId codec_id) {
  switch (codec_id) {
    // video codecs
    case CodecId::AVE_CODEC_ID_H264: {
      return AV_CODEC_ID_H264;
    };
    case CodecId::AVE_CODEC_ID_MPEG4: {
      return AV_CODEC_ID_MPEG4;
    };
    case CodecId::AVE_CODEC_ID_MPEG2VIDEO: {
      return AV_CODEC_ID_MPEG2VIDEO;
    };
    case CodecId::AVE_CODEC_ID_VP8: {
      return AV_CODEC_ID_VP8;
    };
    case CodecId::AVE_CODEC_ID_VP9: {
      return AV_CODEC_ID_VP9;
    };
    case CodecId::AVE_CODEC_ID_HEVC: {
      return AV_CODEC_ID_HEVC;
    };
    case CodecId::AVE_CODEC_ID_AV1: {
      return AV_CODEC_ID_AV1;
    };

    // audio codecs
    case CodecId::AVE_CODEC_ID_PCM_S16LE: {
      return AV_CODEC_ID_PCM_S16LE;
    }
    case CodecId::AVE_CODEC_ID_PCM_S16BE: {
      return AV_CODEC_ID_PCM_S16BE;
    }
    case CodecId::AVE_CODEC_ID_MP3: {
      return AV_CODEC_ID_MP3;
    }
    case CodecId::AVE_CODEC_ID_AAC: {
      return AV_CODEC_ID_AAC;
    }
    case CodecId::AVE_CODEC_ID_AC3: {
      return AV_CODEC_ID_AC3;
    }
    case CodecId::AVE_CODEC_ID_EAC3: {
      return AV_CODEC_ID_EAC3;
    }

    // subtitle codecs
    default: {
      return AV_CODEC_ID_NONE;
    }
  }
}

const char* AVCodecId2Mime(AVCodecID ffmpeg_codec_id) {
  return CodecIdToMime(ConvertToAVECodecId(ffmpeg_codec_id));
}

// pixel format
PixelFormat ConvertFromFFmpegPixelFormat(AVPixelFormat pixel_format) {
  switch (pixel_format) {
    case AV_PIX_FMT_YUV420P:
      return AVE_PIX_FMT_YUV420P;

    case AV_PIX_FMT_YUV422P:
      return AVE_PIX_FMT_YUV422P;

    default:
      AVE_LOG(LS_ERROR) << "Unsupported PixelFormat: " << pixel_format;
  }
  return AVE_PIX_FMT_NONE;
}

AVPixelFormat ConvertToFFmpegPixelFormat(PixelFormat pixel_format) {
  switch (pixel_format) {
    case AVE_PIX_FMT_YUV420P:
      return AV_PIX_FMT_YUV420P;
    case AVE_PIX_FMT_YUV422P:
      return AV_PIX_FMT_YUV422P;
    default:
      AVE_LOG(LS_ERROR) << "Unsupported PixelFormat: " << pixel_format;
  }
  return AV_PIX_FMT_NONE;
}

ChannelLayout ChannelLayoutToAveChannelLayout(uint64_t layout, int channels) {
  switch (layout) {
    case AV_CH_LAYOUT_MONO:
      return CHANNEL_LAYOUT_MONO;
    case AV_CH_LAYOUT_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case AV_CH_LAYOUT_2_1:
      return CHANNEL_LAYOUT_2_1;
    case AV_CH_LAYOUT_SURROUND:
      return CHANNEL_LAYOUT_SURROUND;
    case AV_CH_LAYOUT_4POINT0:
      return CHANNEL_LAYOUT_4_0;
    case AV_CH_LAYOUT_2_2:
      return CHANNEL_LAYOUT_2_2;
    case AV_CH_LAYOUT_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case AV_CH_LAYOUT_5POINT0:
      return CHANNEL_LAYOUT_5_0;
    case AV_CH_LAYOUT_5POINT1:
      return CHANNEL_LAYOUT_5_1;
    case AV_CH_LAYOUT_5POINT0_BACK:
      return CHANNEL_LAYOUT_5_0_BACK;
    case AV_CH_LAYOUT_5POINT1_BACK:
      return CHANNEL_LAYOUT_5_1_BACK;
    case AV_CH_LAYOUT_7POINT0:
      return CHANNEL_LAYOUT_7_0;
    case AV_CH_LAYOUT_7POINT1:
      return CHANNEL_LAYOUT_7_1;
    case AV_CH_LAYOUT_7POINT1_WIDE:
      return CHANNEL_LAYOUT_7_1_WIDE;
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
      return CHANNEL_LAYOUT_STEREO_DOWNMIX;
    default:
      // FFmpeg channel_layout is 0 for .wav and .mp3.  We know mono and
      // stereo from the number of channels, otherwise report errors.
      if (channels == 1) {
        return CHANNEL_LAYOUT_MONO;
      }
      if (channels == 2) {
        return CHANNEL_LAYOUT_STEREO;
      }
      AVE_LOG(LS_DEBUG) << "Unsupported channel layout: " << layout;
  }
  return CHANNEL_LAYOUT_UNSUPPORTED;
}

ChannelLayout ChannelLayoutToAveChannelLayout(const AVChannelLayout& ch_layout,
                                              int channels) {
  if (ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
    return GuessChannelLayout(channels);
  }
  if (ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
    return ChannelLayoutToAveChannelLayout(ch_layout.u.mask, channels);
  }
  if (ch_layout.order == AV_CHANNEL_ORDER_AMBISONIC) {
    // Ambisonic channel layout is not supported yet.
    return CHANNEL_LAYOUT_UNSUPPORTED;
  }
  {
    AVE_LOG(LS_ERROR) << "Unsupported channel layout order: " << ch_layout.order
                      << ", channels: " << channels;
  }
  return CHANNEL_LAYOUT_UNSUPPORTED;
}

void ExtractMetaFromAudioStream(const AVStream* audio_stream,
                                std::shared_ptr<MediaMeta>& meta) {
  // AVE_CHECK_NE(audio_stream, nullptr);
  AVE_CHECK_EQ(audio_stream->codecpar->codec_type, AVMEDIA_TYPE_AUDIO);

  meta->SetStreamType(MediaType::AUDIO);
  meta->SetCodec(ConvertToAVECodecId(audio_stream->codecpar->codec_id));
  meta->SetMime(AVCodecId2Mime(audio_stream->codecpar->codec_id));
  if (audio_stream->codecpar->sample_rate > 0) {
    meta->SetSampleRate(audio_stream->codecpar->sample_rate);
  }
  meta->SetChannelLayout(ChannelLayoutToAveChannelLayout(
      audio_stream->codecpar->ch_layout,
      audio_stream->codecpar->ch_layout.nb_channels));

  if (audio_stream->codecpar->bits_per_coded_sample > 0) {
    meta->SetBitsPerSample(
        static_cast<int16_t>(audio_stream->codecpar->bits_per_coded_sample));
  }
  if (audio_stream->codecpar->extradata) {
    meta->SetPrivateData(audio_stream->codecpar->extradata_size,
                         audio_stream->codecpar->extradata);
  }
}

void ExtractMetaFromVideoStream(const AVStream* video_stream,
                                std::shared_ptr<MediaMeta>& meta) {
  // AVE_CHECK_NE(video_stream, nullptr);
  AVE_CHECK_EQ(video_stream->codecpar->codec_type, AVMEDIA_TYPE_VIDEO);

  meta->SetStreamType(MediaType::VIDEO);
  meta->SetCodec(ConvertToAVECodecId(video_stream->codecpar->codec_id));
  meta->SetMime(AVCodecId2Mime(video_stream->codecpar->codec_id));
  meta->SetWidth(static_cast<int16_t>(video_stream->codecpar->width));
  meta->SetHeight(static_cast<int16_t>(video_stream->codecpar->height));
  meta->SetPixelFormat(ConvertFromFFmpegPixelFormat(
      static_cast<AVPixelFormat>(video_stream->codecpar->format)));

  if (video_stream->time_base.num > 0 && video_stream->time_base.den > 0) {
    meta->SetTimeBase(
        {video_stream->time_base.num, video_stream->time_base.den});
  }

  AVRational aspectRatio = {1, 1};
  if (video_stream->sample_aspect_ratio.num) {
    aspectRatio = video_stream->sample_aspect_ratio;
  } else if (video_stream->codecpar->sample_aspect_ratio.num) {
    aspectRatio = video_stream->codecpar->sample_aspect_ratio;
  }

  meta->SetSampleAspectRatio({aspectRatio.num, aspectRatio.den});

  int32_t profile = video_stream->codecpar->profile;
  if (profile > 0) {
    meta->SetCodecProfile(profile);
  }
  int32_t level = video_stream->codecpar->level;
  if (level > 0) {
    meta->SetCodecLevel(level);
  }

  if (video_stream->codecpar->extradata) {
    meta->SetPrivateData(video_stream->codecpar->extradata_size,
                         video_stream->codecpar->extradata);
  }
}

std::shared_ptr<MediaMeta> ExtractMetaFromAVStream(const AVStream* stream) {
  if (!stream || !stream->codecpar) {
    AVE_LOG(LS_ERROR) << "Invalid AVStream or codecpar";
    return nullptr;
  }

  std::shared_ptr<MediaMeta> meta;

  if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
    meta = MediaMeta::CreatePtr(ave::media::MediaType::AUDIO,
                                MediaMeta::FormatType::kTrack);
    ExtractMetaFromAudioStream(stream, meta);
  } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
    meta = MediaMeta::CreatePtr(ave::media::MediaType::VIDEO,
                                MediaMeta::FormatType::kTrack);
    ExtractMetaFromVideoStream(stream, meta);
  } else {
    return nullptr;
  }

  // Set common properties
  if (stream->duration > 0) {
    meta->SetDuration(base::TimeDelta::Micros(
        ConvertFromTimeBase(stream->time_base, stream->duration)));
  }

  if (stream->codecpar->bit_rate > 0) {
    meta->SetBitrate(stream->codecpar->bit_rate);
  }

  return meta;
}

// avpacket
void ExtractMetaFromAudioPacket(const AVPacket* av_packet,
                                std::shared_ptr<MediaMeta>& meta) {
  //
}

void ExtractMetaFromVideoPacket(const AVPacket* av_packet,
                                std::shared_ptr<MediaMeta>& meta) {
  //
}

std::shared_ptr<MediaFrame> CreateMediaFrameFromAVPacket(
    const AVPacket* av_packet) {
  if (!av_packet) {
    AVE_LOG(LS_ERROR) << "Invalid AVPacket";
    return nullptr;
  }

  auto packet = MediaFrame::CreateShared(av_packet->size);
  packet->SetData(av_packet->data, av_packet->size);

  // packet->SetStreamIndex(av_packet->stream_index);
  packet->meta()->SetPts(base::Timestamp::Micros(
      ConvertFromTimeBase(av_packet->time_base, av_packet->pts)));
  packet->meta()->SetDts(base::Timestamp::Micros(
      ConvertFromTimeBase(av_packet->time_base, av_packet->dts)));
  packet->meta()->SetDuration(base::TimeDelta::Micros(
      ConvertFromTimeBase(av_packet->time_base, av_packet->duration)));
  // packet->meta()->SetFlags(av_packet->flags);

  return packet;
}

void ConfigureAudioCodec(MediaMeta* format, AVCodecContext* codec_context) {
  AVE_DCHECK(format->stream_type() == MediaType::AUDIO);

  codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context->codec_id = ConvertToFFmpegCodecId(format->codec());

  auto bits_per_sample = format->bits_per_sample();
  switch (bits_per_sample) {
    case 8: {
      codec_context->sample_fmt = AV_SAMPLE_FMT_U8;
      break;
    }
    case 16: {
      codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
      break;
    }
    case 32: {
      codec_context->sample_fmt = AV_SAMPLE_FMT_S32;
      break;
    }
    default: {
      AVE_LOG(LS_WARNING) << "Unsupported bits per sample: " << bits_per_sample;
      codec_context->sample_fmt = AV_SAMPLE_FMT_NONE;
    }
  }

  codec_context->sample_rate = static_cast<int>(format->sample_rate());

  auto ch_count = ChannelLayoutToChannelCount(format->channel_layout());
  // codec_context->channels = ch_count;
  av_channel_layout_default(&codec_context->ch_layout, ch_count);
  // TODO: extra data
}

void ConfigureVideoCodec(MediaMeta* format, AVCodecContext* codec_context) {
  AVE_CHECK(format->stream_type() == MediaType::VIDEO);
  codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context->codec_id = ConvertToFFmpegCodecId(format->codec());
  // TODO: profile
  codec_context->profile = FF_PROFILE_UNKNOWN;
  codec_context->coded_width = format->width();
  codec_context->coded_height = format->height();
  codec_context->pix_fmt = ConvertToFFmpegPixelFormat(format->pixel_format());
  // TODO: extra data
}

}  // namespace ffmpeg_utils
}  // namespace media
}  // namespace ave
