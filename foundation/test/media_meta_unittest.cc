/*
 * media_meta_unittest.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "../media_meta.h"
#include <optional>
#include "testing/gtest/include/gtest/gtest.h"

namespace ave {
namespace media {

class MediaFormatTest : public testing::Test {
 protected:
  void SetUp() override {
    track_format_ =
        MediaMeta::Create(MediaType::VIDEO, MediaMeta::FormatType::kTrack);
    sample_format_ =
        MediaMeta::Create(MediaType::VIDEO, MediaMeta::FormatType::kSample);
  }

  std::optional<MediaMeta> track_format_;
  std::optional<MediaMeta> sample_format_;
};

TEST_F(MediaFormatTest, Creation) {
  auto format = MediaMeta::Create();
  EXPECT_EQ(format.stream_type(), MediaType::AUDIO);

  auto ptr_format = MediaMeta::CreatePtr();
  EXPECT_NE(ptr_format, nullptr);
  EXPECT_EQ(ptr_format->stream_type(), MediaType::AUDIO);
}

TEST_F(MediaFormatTest, BasicProperties) {
  // Test stream type
  track_format_->SetStreamType(MediaType::AUDIO);
  EXPECT_EQ(track_format_->stream_type(), MediaType::AUDIO);

  // Test mime
  track_format_->SetMime("video/avc");
  EXPECT_EQ(track_format_->mime(), "video/avc");
  track_format_->SetMime(nullptr);  // Test null case

  // Test name
  track_format_->SetName("test_name");
  EXPECT_EQ(track_format_->name(), "test_name");
  track_format_->SetName(nullptr);  // Test null case

  // Test full name
  track_format_->SetFullName("test_full_name");
  EXPECT_EQ(track_format_->full_name(), "test_full_name");
  track_format_->SetFullName(nullptr);  // Test null case
}

TEST_F(MediaFormatTest, CodecProperties) {
  // Test codec
  track_format_->SetCodec(CodecId::AVE_CODEC_ID_H264);
  EXPECT_EQ(track_format_->codec(), CodecId::AVE_CODEC_ID_H264);

  // Test bitrate
  track_format_->SetBitrate(1000000);
  EXPECT_EQ(track_format_->bitrate(), 1000000);

  // Test duration
  auto duration = base::TimeDelta::Seconds(10);
  track_format_->SetDuration(duration);
  EXPECT_EQ(track_format_->duration(), duration);
}
TEST_F(MediaFormatTest, PrivateData) {
  std::array<uint8_t, 4> test_data = {1, 2, 3, 4};

  // Test setting private data
  track_format_->SetPrivateData(test_data.size(), test_data.data());
  auto buffer = track_format_->private_data();
  EXPECT_NE(buffer, nullptr);
  EXPECT_EQ(buffer->size(), sizeof(test_data));

  // Test null data
  track_format_->SetPrivateData(0, nullptr);

  // Test clear private data
  track_format_->ClearPrivateData();
  EXPECT_EQ(track_format_->private_data(), nullptr);
}

TEST_F(MediaFormatTest, VideoProperties) {
  // Test width/height
  track_format_->SetWidth(1920);
  EXPECT_EQ(track_format_->width(), 1920);
  track_format_->SetHeight(1080);
  EXPECT_EQ(track_format_->height(), 1080);

  // Test stride
  track_format_->SetStride(1920);
  EXPECT_EQ(track_format_->stride(), 1920);

  // Test frame rate
  track_format_->SetFrameRate(30);
  EXPECT_EQ(track_format_->fps(), 30);

  // Test pixel format
  track_format_->SetPixelFormat(PixelFormat::AVE_PIX_FMT_YUV420P);
  EXPECT_EQ(track_format_->pixel_format(), PixelFormat::AVE_PIX_FMT_YUV420P);

  // Test picture type
  sample_format_->SetPictureType(PictureType::I);
  EXPECT_EQ(sample_format_->picture_type(), PictureType::I);

  // Test rotation
  track_format_->SetRotation(90);
  EXPECT_EQ(track_format_->rotation(), 90);

  // Test QP
  sample_format_->SetQp(28);
  EXPECT_EQ(sample_format_->qp(), 28);
}

TEST_F(MediaFormatTest, AudioProperties) {
  auto audio_format =
      MediaMeta::Create(MediaType::AUDIO, MediaMeta::FormatType::kTrack);

  // Test sample rate
  audio_format.SetSampleRate(44100);
  EXPECT_EQ(audio_format.sample_rate(), static_cast<uint32_t>(44100));

  // Test channel layout
  audio_format.SetChannelLayout(CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(audio_format.channel_layout(), CHANNEL_LAYOUT_STEREO);

  // Test samples per channel
  audio_format.SetSamplesPerChannel(1024);
  EXPECT_EQ(audio_format.samples_per_channel(), 1024);

  // Test bits per sample
  audio_format.SetBitsPerSample(16);
  EXPECT_EQ(audio_format.bits_per_sample(), 16);
}

TEST_F(MediaFormatTest, SampleProperties) {
  auto timestamp = base::Timestamp::Seconds(5);

  // Test PTS
  sample_format_->SetPts(timestamp);
  EXPECT_EQ(sample_format_->pts(), timestamp);

  // Test DTS
  sample_format_->SetDts(timestamp);
  EXPECT_EQ(sample_format_->dts(), timestamp);

  // Test EOS
  sample_format_->SetEos(true);
  EXPECT_TRUE(sample_format_->eos());
}

TEST_F(MediaFormatTest, MetaData) {
  auto& meta = track_format_->meta();
  EXPECT_NE(meta, nullptr);
}

TEST_F(MediaFormatTest, InvalidOperations) {
  // Test invalid video operations on audio format
  auto audio_format = MediaMeta::Create(MediaType::AUDIO);
  EXPECT_EQ(audio_format.width(), -1);
  EXPECT_EQ(audio_format.height(), -1);
  EXPECT_EQ(audio_format.stride(), -1);
  EXPECT_EQ(audio_format.fps(), -1);
  EXPECT_EQ(audio_format.rotation(), -1);
  EXPECT_EQ(audio_format.qp(), -1);

  // Test invalid audio operations on video format
  auto video_format = MediaMeta::Create(MediaType::VIDEO);
  EXPECT_EQ(video_format.sample_rate(), static_cast<uint32_t>(0));
  EXPECT_EQ(video_format.channel_layout(), CHANNEL_LAYOUT_NONE);
  EXPECT_EQ(video_format.samples_per_channel(), -1);
  EXPECT_EQ(video_format.bits_per_sample(), -1);

  // Test invalid sample operations on track format
  EXPECT_EQ(track_format_->pts(), base::Timestamp::Zero());
  EXPECT_EQ(track_format_->dts(), base::Timestamp::Zero());
  EXPECT_FALSE(track_format_->eos());
}

}  // namespace media
}  // namespace ave
