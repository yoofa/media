/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/audio_rtp_receive_stream.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "media/foundation/media_sink_base.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"
#include "test/gtest.h"

namespace ave::media::rtp_session {
namespace {

BufferedAudioPacket Packet(uint16_t sequence_number) {
  return BufferedAudioPacket{
      .sequence_number = sequence_number,
      .rtp_timestamp = static_cast<uint32_t>(sequence_number) * 480,
      .payload_type = 96,
      .arrival_time = base::Timestamp::Millis(sequence_number),
      .payload = {static_cast<uint8_t>(sequence_number)},
  };
}

std::vector<uint16_t> Sequences(
    const std::vector<BufferedAudioPacket>& packets) {
  std::vector<uint16_t> sequences;
  for (const BufferedAudioPacket& packet : packets) {
    sequences.push_back(packet.sequence_number);
  }
  return sequences;
}

class RecordingFrameSink final : public MediaFrameSink {
 public:
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override {
    std::scoped_lock lock(mutex_);
    frames_.push_back(frame);
    condition_.notify_all();
  }

  std::shared_ptr<MediaFrame> WaitForFrame() {
    std::unique_lock lock(mutex_);
    if (!condition_.wait_for(lock, std::chrono::seconds(2),
                             [this]() { return !frames_.empty(); })) {
      return nullptr;
    }
    return frames_.front();
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::vector<std::shared_ptr<MediaFrame>> frames_;
};

TEST(AudioPacketBufferTest, ReordersPacketsWithOnePacketTargetDelay) {
  AudioPacketBuffer buffer;
  EXPECT_TRUE(buffer.Insert(Packet(10)).empty());
  EXPECT_EQ(Sequences(buffer.Insert(Packet(12))), (std::vector<uint16_t>{10}));
  EXPECT_EQ(Sequences(buffer.Insert(Packet(11))), (std::vector<uint16_t>{11}));
  EXPECT_EQ(Sequences(buffer.Insert(Packet(13))), (std::vector<uint16_t>{12}));
}

TEST(AudioPacketBufferTest, SkipsLossAndDropsDuplicateAndLatePackets) {
  AudioPacketBuffer buffer;
  EXPECT_TRUE(buffer.Insert(Packet(100)).empty());
  EXPECT_EQ(Sequences(buffer.Insert(Packet(102))),
            (std::vector<uint16_t>{100}));
  EXPECT_TRUE(buffer.Insert(Packet(102)).empty());
  EXPECT_EQ(Sequences(buffer.Insert(Packet(103))),
            (std::vector<uint16_t>{102}));
  EXPECT_TRUE(buffer.Insert(Packet(101)).empty());

  const AudioPacketBufferStats stats = buffer.GetStats();
  EXPECT_EQ(stats.duplicate_packets, 1U);
  EXPECT_EQ(stats.late_packets, 1U);
  EXPECT_EQ(stats.skipped_packets, 1U);
  EXPECT_LE(stats.buffered_packets, 1U);
}

TEST(AudioPacketBufferTest, HandlesSequenceNumberRollover) {
  AudioPacketBuffer buffer;
  EXPECT_TRUE(buffer.Insert(Packet(65535)).empty());
  EXPECT_EQ(Sequences(buffer.Insert(Packet(0))),
            (std::vector<uint16_t>{65535}));
  EXPECT_EQ(Sequences(buffer.Insert(Packet(1))), (std::vector<uint16_t>{0}));
}

TEST(AudioRtpReceiveStreamTest, EmitsOrderedEncodedMediaFrames) {
  AudioRtpReceiveStream::Config config{
      .remote_ssrc = 1234,
      .rtp_clock_rate_hz = 48000,
      .default_frame_duration_ms = 20,
      .payload_type_codecs = {{96, CodecId::AVE_CODEC_ID_OPUS}},
  };
  AudioRtpReceiveStream stream(config);
  RecordingFrameSink sink;
  stream.AddOrUpdateSink(&sink, {});
  stream.Start();

  auto create_rtp_packet = [](uint16_t sequence_number, uint32_t timestamp,
                              uint8_t value) {
    rtp_rtcp::RtpPacketReceived packet;
    packet.SetSsrc(1234);
    packet.SetPayloadType(96);
    packet.SetSequenceNumber(sequence_number);
    packet.SetTimestamp(timestamp);
    packet.set_arrival_time(base::Timestamp::Millis(sequence_number));
    uint8_t* payload = packet.AllocatePayload(1);
    *payload = value;
    return packet;
  };

  stream.OnRtpPacket(create_rtp_packet(7, 1000, 42));
  stream.OnRtpPacket(create_rtp_packet(8, 1480, 43));
  const std::shared_ptr<MediaFrame> frame = sink.WaitForFrame();
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->codec(), CodecId::AVE_CODEC_ID_OPUS);
  EXPECT_EQ(frame->pts(), base::Timestamp::Zero());
  EXPECT_EQ(frame->duration(), base::TimeDelta::Millis(10));
  EXPECT_EQ(frame->arrival_time(), base::Timestamp::Millis(7));
  ASSERT_EQ(frame->size(), 1U);
  EXPECT_EQ(frame->data()[0], 42U);

  stream.Stop();
  stream.RemoveSink(&sink);
}

}  // namespace
}  // namespace ave::media::rtp_session
