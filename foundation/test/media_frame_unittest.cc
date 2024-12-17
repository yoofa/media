/*
 * media_frame_unittest.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "../media_frame.h"
#include "test/gtest.h"

namespace ave {
namespace media {

namespace {
const size_t kFrameSize = 1024;
const char* kTestData = "test frame data";

// Default audio sample info
const CodecId kDefaultAudioCodecId = CodecId::AVE_CODEC_ID_AAC;
const int64_t kDefaultSampleRate = 44100;
const ChannelLayout kDefaultChannelLayout = CHANNEL_LAYOUT_STEREO;
const int64_t kDefaultSamplesPerChannel = 1024;
const int16_t kDefaultBitsPerSample = 16;
const base::Timestamp kDefaultAudioTimestamp = base::Timestamp::Millis(1000);

// Default video sample info
const CodecId kDefaultVideoCodecId = CodecId::AVE_CODEC_ID_H264;
const int16_t kDefaultWidth = 1920;
const int16_t kDefaultHeight = 1080;
const int16_t kDefaultStride = 1920;
const int16_t kDefaultRotation = 0;
const PixelFormat kDefaultPixelFormat = PixelFormat::AVE_PIX_FMT_YUV420P;
const PictureType kDefaultPictureType = PictureType::I;
const int16_t kDefaultQp = 28;
const base::Timestamp kDefaultVideoTimestamp = base::Timestamp::Millis(1000);
}  // namespace

TEST(MediaFrameTest, BasicDataTest) {
  MediaFrame frame = MediaFrame::Create(kFrameSize);
  EXPECT_EQ(frame.buffer_type(), MediaFrame::FrameBufferType::kTypeNormal);
  EXPECT_EQ(frame.size(), kFrameSize);

  frame.SetSize(strlen(kTestData));
  EXPECT_EQ(frame.size(), strlen(kTestData));

  frame.SetData((uint8_t*)kTestData, strlen(kTestData));
  EXPECT_EQ(frame.size(), strlen(kTestData));
  EXPECT_EQ(memcmp(frame.data(), kTestData, strlen(kTestData)), 0);

  // Test copy constructor
  MediaFrame copy = frame;
  EXPECT_EQ(copy.size(), strlen(kTestData));
  EXPECT_EQ(memcmp(copy.data(), kTestData, strlen(kTestData)), 0);
}

TEST(MediaFrameTest, NativeHandleTest) {
  void* test_handle = (void*)0x12345678;
  MediaFrame frame = MediaFrame::CreateWithHandle(test_handle);
  EXPECT_EQ(frame.buffer_type(),
            MediaFrame::FrameBufferType::kTypeNativeHandle);
  EXPECT_EQ(frame.size(), (size_t)0);
  EXPECT_EQ(frame.data(), nullptr);
  EXPECT_EQ(frame.native_handle(), test_handle);

  // Test copy constructor with native handle
  MediaFrame copy = frame;
  EXPECT_EQ(copy.buffer_type(), MediaFrame::FrameBufferType::kTypeNativeHandle);
  EXPECT_EQ(copy.size(), (size_t)0);
  EXPECT_EQ(copy.data(), nullptr);
  EXPECT_EQ(copy.native_handle(), test_handle);
}

TEST(MediaFrameTest, MediaTypeTest) {
  MediaFrame frame = MediaFrame::Create(kFrameSize);

  // Default type should be unknown
  EXPECT_EQ(frame.GetMediaType(), MediaType::UNKNOWN);
  EXPECT_EQ(frame.audio_info(), nullptr);
  EXPECT_EQ(frame.video_info(), nullptr);

  // Test audio type
  frame.SetMediaType(MediaType::AUDIO);
  EXPECT_EQ(frame.GetMediaType(), MediaType::AUDIO);
  EXPECT_NE(frame.audio_info(), nullptr);
  EXPECT_EQ(frame.video_info(), nullptr);

  // Test video type
  frame.SetMediaType(MediaType::VIDEO);
  EXPECT_EQ(frame.GetMediaType(), MediaType::VIDEO);
  EXPECT_EQ(frame.audio_info(), nullptr);
  EXPECT_NE(frame.video_info(), nullptr);
}

TEST(MediaFrameTest, AudioSampleInfoTest) {
  MediaFrame frame = MediaFrame::Create(kFrameSize);
  frame.SetMediaType(MediaType::AUDIO);

  auto* audio_info = frame.audio_info();
  ASSERT_NE(audio_info, nullptr);

  audio_info->codec_id = kDefaultAudioCodecId;
  audio_info->sample_rate_hz = kDefaultSampleRate;
  audio_info->channel_layout = kDefaultChannelLayout;
  audio_info->samples_per_channel = kDefaultSamplesPerChannel;
  audio_info->bits_per_sample = kDefaultBitsPerSample;
  audio_info->pts = kDefaultAudioTimestamp;

  // Test copy constructor preserves audio info
  MediaFrame copy = frame;
  auto* copy_info = copy.audio_info();
  ASSERT_NE(copy_info, nullptr);
  EXPECT_EQ(copy_info->codec_id, kDefaultAudioCodecId);
  EXPECT_EQ(copy_info->sample_rate_hz, kDefaultSampleRate);
  EXPECT_EQ(copy_info->channel_layout, kDefaultChannelLayout);
  EXPECT_EQ(copy_info->samples_per_channel, kDefaultSamplesPerChannel);
  EXPECT_EQ(copy_info->bits_per_sample, kDefaultBitsPerSample);
  EXPECT_EQ(copy_info->pts, kDefaultAudioTimestamp);
}

TEST(MediaFrameTest, VideoSampleInfoTest) {
  MediaFrame frame = MediaFrame::Create(kFrameSize);
  frame.SetMediaType(MediaType::VIDEO);

  auto* video_info = frame.video_info();
  ASSERT_NE(video_info, nullptr);

  video_info->codec_id = kDefaultVideoCodecId;
  video_info->width = kDefaultWidth;
  video_info->height = kDefaultHeight;
  video_info->stride = kDefaultStride;
  video_info->rotation = kDefaultRotation;
  video_info->pixel_format = kDefaultPixelFormat;
  video_info->picture_type = kDefaultPictureType;
  video_info->qp = kDefaultQp;
  video_info->pts = kDefaultVideoTimestamp;

  // Test copy constructor preserves video info
  MediaFrame copy = frame;
  auto* copy_info = copy.video_info();
  ASSERT_NE(copy_info, nullptr);
  EXPECT_EQ(copy_info->codec_id, kDefaultVideoCodecId);
  EXPECT_EQ(copy_info->width, kDefaultWidth);
  EXPECT_EQ(copy_info->height, kDefaultHeight);
  EXPECT_EQ(copy_info->stride, kDefaultStride);
  EXPECT_EQ(copy_info->rotation, kDefaultRotation);
  EXPECT_EQ(copy_info->pixel_format, kDefaultPixelFormat);
  EXPECT_EQ(copy_info->picture_type, kDefaultPictureType);
  EXPECT_EQ(copy_info->qp, kDefaultQp);
  EXPECT_EQ(copy_info->pts, kDefaultVideoTimestamp);
}

}  // namespace media
}  // namespace ave
