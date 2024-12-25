/*
 * media_utils_unittest.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "../media_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ave {
namespace media {

TEST(MediaUtilsTest, GetMediaTypeString) {
  EXPECT_STREQ("video", get_media_type_string(MediaType::VIDEO));
  EXPECT_STREQ("audio", get_media_type_string(MediaType::AUDIO));
  EXPECT_STREQ("data", get_media_type_string(MediaType::DATA));
  EXPECT_STREQ("subtitle", get_media_type_string(MediaType::SUBTITLE));
  EXPECT_STREQ("attachment", get_media_type_string(MediaType::ATTACHMENT));
  EXPECT_EQ(nullptr, get_media_type_string(MediaType::UNKNOWN));
}

TEST(MediaUtilsTest, CodecMediaType) {
  // Test video codecs
  EXPECT_EQ(MediaType::VIDEO,
            CodecMediaType(CodecId::AVE_CODEC_ID_FIRST_VIDEO));
  EXPECT_EQ(MediaType::VIDEO, CodecMediaType(CodecId::AVE_CODEC_ID_LAST_VIDEO));

  // Test audio codecs
  EXPECT_EQ(MediaType::AUDIO,
            CodecMediaType(CodecId::AVE_CODEC_ID_FIRST_AUDIO));
  EXPECT_EQ(MediaType::AUDIO, CodecMediaType(CodecId::AVE_CODEC_ID_LAST_AUDIO));

  // Test subtitle codecs
  EXPECT_EQ(MediaType::SUBTITLE,
            CodecMediaType(CodecId::AVE_CODEC_ID_FIRST_SUBTITLE));
  EXPECT_EQ(MediaType::SUBTITLE,
            CodecMediaType(CodecId::AVE_CODEC_ID_LAST_SUBTITLE));

  // Test unknown codec
  EXPECT_EQ(MediaType::UNKNOWN, CodecMediaType(CodecId::AVE_CODEC_ID_NONE));
}

TEST(MediaUtilsTest, MediaSampleInfo) {
  // Test Audio Sample Info
  MediaSampleInfo audio_sample(MediaType::AUDIO);
  EXPECT_EQ(audio_sample.sample_type, MediaType::AUDIO);

  auto& audio_info = audio_sample.audio();
  EXPECT_EQ(audio_info.codec_id, CodecId::AVE_CODEC_ID_NONE);
  EXPECT_EQ(audio_info.sample_rate_hz, -1);
  EXPECT_EQ(audio_info.channel_layout, CHANNEL_LAYOUT_NONE);
  EXPECT_EQ(audio_info.samples_per_channel, -1);
  EXPECT_EQ(audio_info.bits_per_sample, -1);
  EXPECT_EQ(audio_info.pts, base::Timestamp::Zero());
  EXPECT_EQ(audio_info.dts, base::Timestamp::Zero());
  EXPECT_EQ(audio_info.duration, base::TimeDelta::Zero());
  EXPECT_FALSE(audio_info.eos);
  EXPECT_EQ(audio_info.private_data, nullptr);

  // Test Video Sample Info
  MediaSampleInfo video_sample(MediaType::VIDEO);
  EXPECT_EQ(video_sample.sample_type, MediaType::VIDEO);

  auto& video_info = video_sample.video();
  EXPECT_EQ(video_info.codec_id, CodecId::AVE_CODEC_ID_NONE);
  EXPECT_EQ(video_info.stride, -1);
  EXPECT_EQ(video_info.width, -1);
  EXPECT_EQ(video_info.height, -1);
  EXPECT_EQ(video_info.rotation, -1);
  EXPECT_EQ(video_info.pts, base::Timestamp::Zero());
  EXPECT_EQ(video_info.dts, base::Timestamp::Zero());
  EXPECT_EQ(video_info.duration, base::TimeDelta::Zero());
  EXPECT_FALSE(video_info.eos);
  EXPECT_EQ(video_info.pixel_format, PixelFormat::AVE_PIX_FMT_NONE);
  EXPECT_EQ(video_info.picture_type, PictureType::NONE);
  EXPECT_EQ(video_info.qp, -1);
  EXPECT_EQ(video_info.private_data, nullptr);

  // Test Other Sample Info
  MediaSampleInfo other_sample(MediaType::DATA);
  EXPECT_EQ(other_sample.sample_type, MediaType::DATA);
}

TEST(MediaUtilsTest, MediaTrackInfo) {
  // Test Audio Track Info
  MediaTrackInfo audio_track(MediaType::AUDIO);
  EXPECT_EQ(audio_track.track_type, MediaType::AUDIO);

  auto& audio_info = audio_track.audio();
  EXPECT_EQ(audio_info.codec_id, CodecId::AVE_CODEC_ID_NONE);
  EXPECT_EQ(audio_info.duration, base::TimeDelta::Zero());
  EXPECT_EQ(audio_info.bitrate_bps, -1);
  EXPECT_EQ(audio_info.sample_rate_hz, -1);
  EXPECT_EQ(audio_info.channel_layout, CHANNEL_LAYOUT_NONE);
  EXPECT_EQ(audio_info.samples_per_channel, -1);
  EXPECT_EQ(audio_info.bits_per_sample, -1);
  EXPECT_EQ(audio_info.private_data, nullptr);

  // Test Video Track Info
  MediaTrackInfo video_track(MediaType::VIDEO);
  EXPECT_EQ(video_track.track_type, MediaType::VIDEO);

  auto& video_info = video_track.video();
  EXPECT_EQ(video_info.codec_id, CodecId::AVE_CODEC_ID_NONE);
  EXPECT_EQ(video_info.duration, base::TimeDelta::Zero());
  EXPECT_EQ(video_info.bitrate_bps, -1);
  EXPECT_EQ(video_info.stride, -1);
  EXPECT_EQ(video_info.width, -1);
  EXPECT_EQ(video_info.height, -1);
  EXPECT_EQ(video_info.rotation, -1);
  EXPECT_EQ(video_info.pixel_format, PixelFormat::AVE_PIX_FMT_NONE);
  EXPECT_EQ(video_info.fps, -1);
  EXPECT_EQ(video_info.private_data, nullptr);

  // Test Other Track Info
  MediaTrackInfo other_track(MediaType::DATA);
  EXPECT_EQ(other_track.track_type, MediaType::DATA);
}

}  // namespace media
}  // namespace ave
