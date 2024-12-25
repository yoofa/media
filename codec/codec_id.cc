/*
 * codec_id.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "codec_id.h"

#include <cstring>

#include "../foundation/media_defs.h"

namespace ave {

namespace media {

// TODO: not complete, refine when check more test files

CodecId MimeToCodecId(const char* mime) {
  // VIDEO
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_VP8, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_VP8;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_VP9, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_VP9;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_AV1, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_AV1;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_AVC, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_H264;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_HEVC, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_HEVC;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_MPEG4, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_MPEG4;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_H263, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_H263;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_MPEG2, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_MPEG2VIDEO;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_RAW, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_RAWVIDEO;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_DOLBY_VISION, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_HEVC;
  }
  //   if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_DIVX, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_DIVX;
  //   }
  //   if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_DIVX3, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_DIVX3;
  //   }
  //   if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_XVID, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_XVID;
  //   }
  if (!strncasecmp(MEDIA_MIMETYPE_VIDEO_MJPEG, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_MJPEG;
  }
  // AUDIO
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_AMR_NB, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_AMR_NB;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_AMR_WB, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_AMR_WB;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MPEG, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_MP3;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_I, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_MP3;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_II, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_MP2;
  }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MIDI, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_MIDI;
  //   }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_AAC, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_AAC;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_QCELP, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_QCELP;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_VORBIS, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_VORBIS;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_OPUS, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_OPUS;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_G711_ALAW, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_PCM_ALAW;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_G711_MLAW, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_PCM_MULAW;
  }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_RAW, mime, strlen(mime))) {
  //     return AUDIO_RAW;
  //   }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_FLAC, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_FLAC;
  }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_AAC_ADTS, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_AAC_ADTS;
  //   }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MSGSM, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_GSM_MS;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_AC3, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_AC3;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_EAC3, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_EAC3;
  }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_EAC3_JOC, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_EAC3_JOC;
  //   }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_AC4, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_AC4;
  //   }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MPEGH_MHA1, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_MPEGH_MHA1;
  //   }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MPEGH_MHM1, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_MPEGH_MHM1;
  //   }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_ALAC, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_ALAC;
  }
  if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_WMA, mime, strlen(mime))) {
    return CodecId::AVE_CODEC_ID_WMAV1;
  }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_MS_ADPCM, mime, strlen(mime))) {
  //     return CodecId::AVE_CODEC_ID_MSADPCM;
  //   }
  //   if (!strncasecmp(MEDIA_MIMETYPE_AUDIO_DVI_IMA_ADPCM, mime, strlen(mime)))
  //   {
  //     return CodecId::AVE_CODEC_ID_DVI_IMA_ADPCM;
  //   }
  // UNKNOWN
  return CodecId::AVE_CODEC_ID_NONE;
}

const char* CodecIdToMime(CodecId codec_id) {
  switch (codec_id) {
    case CodecId::AVE_CODEC_ID_VP8:
      return MEDIA_MIMETYPE_VIDEO_VP8;
    case CodecId::AVE_CODEC_ID_VP9:
      return MEDIA_MIMETYPE_VIDEO_VP9;
    case CodecId::AVE_CODEC_ID_AV1:
      return MEDIA_MIMETYPE_VIDEO_AV1;
    case CodecId::AVE_CODEC_ID_H264:
      return MEDIA_MIMETYPE_VIDEO_AVC;
    case CodecId::AVE_CODEC_ID_HEVC:
      return MEDIA_MIMETYPE_VIDEO_HEVC;
    case CodecId::AVE_CODEC_ID_MPEG4:
      return MEDIA_MIMETYPE_VIDEO_MPEG4;
    case CodecId::AVE_CODEC_ID_H263:
      return MEDIA_MIMETYPE_VIDEO_H263;
    case CodecId::AVE_CODEC_ID_MPEG2VIDEO:
      return MEDIA_MIMETYPE_VIDEO_MPEG2;
    case CodecId::AVE_CODEC_ID_RAWVIDEO:
      return MEDIA_MIMETYPE_VIDEO_RAW;
    case CodecId::AVE_CODEC_ID_MJPEG:
      return MEDIA_MIMETYPE_VIDEO_MJPEG;
    case CodecId::AVE_CODEC_ID_AMR_NB:
      return MEDIA_MIMETYPE_AUDIO_AMR_NB;
    case CodecId::AVE_CODEC_ID_AMR_WB:
      return MEDIA_MIMETYPE_AUDIO_AMR_WB;
    case CodecId::AVE_CODEC_ID_MP3:
      return MEDIA_MIMETYPE_AUDIO_MPEG;
    case CodecId::AVE_CODEC_ID_MP2:
      return MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_II;
    case CodecId::AVE_CODEC_ID_AAC:
      return MEDIA_MIMETYPE_AUDIO_AAC;
    case CodecId::AVE_CODEC_ID_QCELP:
      return MEDIA_MIMETYPE_AUDIO_QCELP;
    case CodecId::AVE_CODEC_ID_VORBIS:
      return MEDIA_MIMETYPE_AUDIO_VORBIS;
    case CodecId::AVE_CODEC_ID_OPUS:
      return MEDIA_MIMETYPE_AUDIO_OPUS;
    case CodecId::AVE_CODEC_ID_PCM_ALAW:
      return MEDIA_MIMETYPE_AUDIO_G711_ALAW;
    case CodecId::AVE_CODEC_ID_PCM_MULAW:
      return MEDIA_MIMETYPE_AUDIO_G711_MLAW;
    case CodecId::AVE_CODEC_ID_FLAC:
      return MEDIA_MIMETYPE_AUDIO_FLAC;
    case CodecId::AVE_CODEC_ID_GSM_MS:
      return MEDIA_MIMETYPE_AUDIO_MSGSM;
    case CodecId::AVE_CODEC_ID_AC3:
      return MEDIA_MIMETYPE_AUDIO_AC3;
    case CodecId::AVE_CODEC_ID_EAC3:
      return MEDIA_MIMETYPE_AUDIO_EAC3;
    case CodecId::AVE_CODEC_ID_ALAC:
      return MEDIA_MIMETYPE_AUDIO_ALAC;
    case CodecId::AVE_CODEC_ID_WMAV1:
      return MEDIA_MIMETYPE_AUDIO_WMA;
    default:
      return "";
  }
}

}  // namespace media
}  // namespace ave
