/*
 * media_packet_unittest.cc
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "../media_packet.h"
#include "../media_utils.h"

#include "base/units/timestamp.h"

#include "test/gtest.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace ave {
namespace media {

namespace {
const size_t kSampleCount = 10;
const char* kTestString = "hello world";

// default audio sample info
const CodecId DefaultAudioCodecId = CodecId::AVE_CODEC_ID_AAC;
const base::Timestamp DefaultAudioTimeStamp = base::Timestamp::Millis(1000);
const int64_t DefaultAudioSampleRate = 44100;
const ChannelLayout DefaultAudioChannelLayout =
    ChannelLayout::CHANNEL_LAYOUT_STEREO;
const int64_t DefaultAudioSamplePerChannel = 480;
const int16_t DefaultAudioBitsPerSample = 16;

// default video sample info
const CodecId DefaultVideoCodecId = CodecId::AVE_CODEC_ID_MPEG1VIDEO;
const base::Timestamp DefaultVideoTimeStamp = base::Timestamp::Millis(1000);
const base::Timestamp DefaultVideoDts = base::Timestamp::Millis(1000);
const int16_t DefaultVideoWidth = 1920;
const int16_t DefaultVideoHeight = 1080;
const int16_t DefaultVideoStride = 1920;
const int16_t DefaultVideoRotation = 0;
const PixelFormat DefaultVideoPixelFormat = PixelFormat::AVE_PIX_FMT_YUV420P;
const PictureType DefaultVideoPictureTyoe = PictureType::B;
const int16_t DefaultVideoQP = 25;

}  // namespace

TEST(MediaPacketTest, BasicDataTest) {
  MediaPacket packet = MediaPacket::Create(kSampleCount);
  EXPECT_EQ(packet.buffer_type(), MediaPacket::PacketBufferType::kTypeNormal);
  EXPECT_EQ(packet.size(), kSampleCount);

  packet.SetSize(2);
  EXPECT_EQ(packet.size(), (size_t)2);

  packet.SetData((uint8_t*)kTestString, strlen(kTestString));
  EXPECT_EQ(packet.size(), strlen(kTestString));
  EXPECT_EQ(memcmp(packet.data(), kTestString, strlen(kTestString)), 0);

  // copy packet
  MediaPacket copy = packet;
  EXPECT_EQ(copy.size(), strlen(kTestString));
  EXPECT_EQ(memcmp(copy.data(), kTestString, strlen(kTestString)), 0);
}

TEST(MediaPacketTest, BasicNativeHandleTest) {
  MediaPacket packet = MediaPacket::CreateWithHandle((void*)kTestString);
  EXPECT_EQ(packet.buffer_type(),
            MediaPacket::PacketBufferType::kTypeNativeHandle);
  EXPECT_EQ(packet.size(), (size_t)0);
  EXPECT_EQ(packet.data(), nullptr);

  void* handle = packet.native_handle();
  EXPECT_EQ(handle, (void*)kTestString);

  MediaPacket copy = packet;
  EXPECT_EQ(copy.size(), (size_t)0);
  EXPECT_EQ(copy.data(), nullptr);
  EXPECT_EQ(copy.native_handle(), (void*)kTestString);
}

TEST(MediaPacketTest, SampleInfoBasicTest) {
  MediaPacket packet = MediaPacket::Create(kSampleCount);

  // default packet type is unknown
  EXPECT_EQ(packet.media_type(), MediaType::UNKNOWN);
  auto* audio_info = packet.audio_info();
  EXPECT_EQ(audio_info, nullptr);
  auto* video_info = packet.video_info();
  EXPECT_EQ(video_info, nullptr);

  // set packet type to audio
  packet.SetMediaType(MediaType::AUDIO);
  audio_info = packet.audio_info();
  EXPECT_NE(audio_info, nullptr);
  video_info = packet.video_info();
  EXPECT_EQ(video_info, nullptr);

  // set packet type to video
  packet.SetMediaType(MediaType::VIDEO);
  audio_info = packet.audio_info();
  EXPECT_EQ(audio_info, nullptr);
  video_info = packet.video_info();
  EXPECT_NE(video_info, nullptr);
}

TEST(MediaPacketTest, AudioInfoCopyTest) {
  MediaPacket packet = MediaPacket::Create(kSampleCount);
  packet.SetMediaType(MediaType::AUDIO);
  auto* audio_info = packet.audio_info();
  EXPECT_NE(audio_info, nullptr);
  audio_info->codec_id = DefaultAudioCodecId;
  audio_info->pts = DefaultAudioTimeStamp;
  audio_info->sample_rate_hz = DefaultAudioSampleRate;
  audio_info->channel_layout = DefaultAudioChannelLayout;
  audio_info->samples_per_channel = DefaultAudioSamplePerChannel;
  audio_info->bits_per_sample = DefaultAudioBitsPerSample;

  MediaPacket copy = packet;
  EXPECT_EQ(copy.media_type(), MediaType::AUDIO);
  auto* copy_packet_info = copy.audio_info();
  EXPECT_NE(copy_packet_info, nullptr);
  EXPECT_EQ(copy_packet_info->codec_id, DefaultAudioCodecId);
  EXPECT_EQ(copy_packet_info->pts, DefaultAudioTimeStamp);
  EXPECT_EQ(copy_packet_info->sample_rate_hz, DefaultAudioSampleRate);
  EXPECT_EQ(copy_packet_info->channel_layout, DefaultAudioChannelLayout);
  EXPECT_EQ(copy_packet_info->samples_per_channel,
            DefaultAudioSamplePerChannel);
  EXPECT_EQ(copy_packet_info->bits_per_sample, DefaultAudioBitsPerSample);
}

TEST(MediaPacketTest, VideoInfoCopyTest) {
  MediaPacket packet = MediaPacket::Create(kSampleCount);
  packet.SetMediaType(MediaType::VIDEO);
  auto* video_info = packet.video_info();
  EXPECT_NE(video_info, nullptr);
  video_info->codec_id = DefaultVideoCodecId;
  video_info->pts = DefaultVideoTimeStamp;
  video_info->dts = DefaultVideoDts;
  video_info->width = DefaultVideoWidth;
  video_info->height = DefaultVideoHeight;
  video_info->stride = DefaultVideoStride;
  video_info->rotation = DefaultVideoRotation;
  video_info->pixel_format = DefaultVideoPixelFormat;
  video_info->picture_type = DefaultVideoPictureTyoe;
  video_info->qp = DefaultVideoQP;

  MediaPacket copy = packet;
  EXPECT_EQ(copy.media_type(), MediaType::VIDEO);
  auto* copy_packet_info = copy.video_info();
  EXPECT_NE(copy_packet_info, nullptr);

  EXPECT_EQ(copy_packet_info->codec_id, DefaultVideoCodecId);
  EXPECT_EQ(copy_packet_info->pts, DefaultVideoTimeStamp);
  EXPECT_EQ(copy_packet_info->dts, DefaultVideoDts);
  EXPECT_EQ(copy_packet_info->width, DefaultVideoWidth);
  EXPECT_EQ(copy_packet_info->height, DefaultVideoHeight);
  EXPECT_EQ(copy_packet_info->stride, DefaultVideoStride);
  EXPECT_EQ(copy_packet_info->rotation, DefaultVideoRotation);
  EXPECT_EQ(copy_packet_info->pixel_format, DefaultVideoPixelFormat);
  EXPECT_EQ(copy_packet_info->picture_type, DefaultVideoPictureTyoe);
  EXPECT_EQ(copy_packet_info->qp, DefaultVideoQP);
}

}  // namespace media
}  // namespace ave
