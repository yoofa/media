/*
 * codec_id_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/codec/codec_id.h"

#include <cstring>

#include "test/gtest.h"

namespace ave {
namespace media {

TEST(CodecIdTest, MimeToCodecIdVideo) {
  EXPECT_EQ(MimeToCodecId("video/avc"), CodecId::AVE_CODEC_ID_H264);
  EXPECT_EQ(MimeToCodecId("video/hevc"), CodecId::AVE_CODEC_ID_HEVC);
  EXPECT_EQ(MimeToCodecId("video/x-vnd.on2.vp8"), CodecId::AVE_CODEC_ID_VP8);
  EXPECT_EQ(MimeToCodecId("video/x-vnd.on2.vp9"), CodecId::AVE_CODEC_ID_VP9);
  EXPECT_EQ(MimeToCodecId("video/av01"), CodecId::AVE_CODEC_ID_AV1);
  EXPECT_EQ(MimeToCodecId("video/mp4v-es"), CodecId::AVE_CODEC_ID_MPEG4);
  EXPECT_EQ(MimeToCodecId("video/3gpp"), CodecId::AVE_CODEC_ID_H263);
}

TEST(CodecIdTest, MimeToCodecIdAudio) {
  EXPECT_EQ(MimeToCodecId("audio/mp4a-latm"), CodecId::AVE_CODEC_ID_AAC);
  EXPECT_EQ(MimeToCodecId("audio/mpeg"), CodecId::AVE_CODEC_ID_MP3);
  EXPECT_EQ(MimeToCodecId("audio/opus"), CodecId::AVE_CODEC_ID_OPUS);
  EXPECT_EQ(MimeToCodecId("audio/flac"), CodecId::AVE_CODEC_ID_FLAC);
  EXPECT_EQ(MimeToCodecId("audio/vorbis"), CodecId::AVE_CODEC_ID_VORBIS);
  EXPECT_EQ(MimeToCodecId("audio/3gpp"), CodecId::AVE_CODEC_ID_AMR_NB);
  EXPECT_EQ(MimeToCodecId("audio/amr-wb"), CodecId::AVE_CODEC_ID_AMR_WB);
}

TEST(CodecIdTest, MimeToCodecIdUnknown) {
  EXPECT_EQ(MimeToCodecId("video/unknown"), CodecId::AVE_CODEC_ID_NONE);
}

TEST(CodecIdTest, CodecIdToMimeVideo) {
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_H264), "video/avc");
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_HEVC), "video/hevc");
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_VP8), "video/x-vnd.on2.vp8");
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_VP9), "video/x-vnd.on2.vp9");
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_AV1), "video/av01");
}

TEST(CodecIdTest, CodecIdToMimeAudio) {
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_AAC), "audio/mp4a-latm");
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_MP3), "audio/mpeg");
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_OPUS), "audio/opus");
  EXPECT_STREQ(CodecIdToMime(CodecId::AVE_CODEC_ID_FLAC), "audio/flac");
}

TEST(CodecIdTest, CodecIdToMimeUnknown) {
  const char* result = CodecIdToMime(CodecId::AVE_CODEC_ID_NONE);
  EXPECT_STREQ(result, "");
}

TEST(CodecIdTest, RoundTripConversion) {
  // Video codecs
  CodecId video_ids[] = {
      CodecId::AVE_CODEC_ID_H264, CodecId::AVE_CODEC_ID_HEVC,
      CodecId::AVE_CODEC_ID_VP8,  CodecId::AVE_CODEC_ID_VP9,
      CodecId::AVE_CODEC_ID_AV1,  CodecId::AVE_CODEC_ID_MPEG4,
      CodecId::AVE_CODEC_ID_H263, CodecId::AVE_CODEC_ID_MPEG2VIDEO,
  };

  for (auto id : video_ids) {
    const char* mime = CodecIdToMime(id);
    EXPECT_NE(strlen(mime), 0u)
        << "Empty mime for codec id " << static_cast<int>(id);
    CodecId round_trip = MimeToCodecId(mime);
    EXPECT_EQ(round_trip, id) << "Round trip failed for " << mime;
  }

  // Audio codecs
  CodecId audio_ids[] = {
      CodecId::AVE_CODEC_ID_AAC,    CodecId::AVE_CODEC_ID_MP3,
      CodecId::AVE_CODEC_ID_OPUS,   CodecId::AVE_CODEC_ID_FLAC,
      CodecId::AVE_CODEC_ID_VORBIS, CodecId::AVE_CODEC_ID_AMR_NB,
      CodecId::AVE_CODEC_ID_AMR_WB,
  };

  for (auto id : audio_ids) {
    const char* mime = CodecIdToMime(id);
    EXPECT_NE(strlen(mime), 0u)
        << "Empty mime for codec id " << static_cast<int>(id);
    CodecId round_trip = MimeToCodecId(mime);
    EXPECT_EQ(round_trip, id) << "Round trip failed for " << mime;
  }
}

}  // namespace media
}  // namespace ave
