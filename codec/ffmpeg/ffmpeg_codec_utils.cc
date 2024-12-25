/*
 * ffmpeg_codec_utils.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "ffmpeg_codec_utils.h"

#include <memory>

#include "../../audio/channel_layout.h"
#include "base/attributes.h"
#include "base/checks.h"
#include "third_party/ffmpeg/libavcodec/codec_id.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace ave {
namespace media {

static const AVRational kMicrosBase = {1, 1000000};

int64_t ConvertFromTimeBase(const AVRational& time_base, int64_t pkt_pts) {
  return av_rescale_q(pkt_pts, time_base, kMicrosBase);
}

int64_t ConvertToTimeBase(const AVRational& time_base, const int64_t time_us) {
  return av_rescale_q(time_us, kMicrosBase, time_base);
}

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

CodecId ConvertToAVECodecId(const AVCodecID& ffmpeg_codec_id) {
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

AVCodecID ConvertToFFmpegCodecId(const CodecId& codec_id) {
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

void ConfigureAudioCodec(MediaFormat* format, AVCodecContext* codec_context) {
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

void ConfigureVideoCodec(MediaFormat* format, AVCodecContext* codec_context) {
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

}  // namespace media
}  // namespace ave
