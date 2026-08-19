/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/video_rtp_receive_stream.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "media/foundation/media_sink_base.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"
#include "test/gtest.h"

namespace ave::media::rtp_session {
namespace {

class RecordingVideoSink final : public MediaFrameSink {
 public:
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override {
    std::scoped_lock lock(mutex_);
    frames_.push_back(frame);
    condition_.notify_all();
  }

  std::vector<std::shared_ptr<MediaFrame>> WaitForFrames(size_t count) {
    std::unique_lock lock(mutex_);
    condition_.wait_for(lock, std::chrono::seconds(2),
                        [this, count]() { return frames_.size() >= count; });
    return frames_;
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::vector<std::shared_ptr<MediaFrame>> frames_;
};

rtp_rtcp::RtpPacketReceived Packet(uint16_t sequence,
                                   bool marker,
                                   std::span<const uint8_t> payload,
                                   uint32_t timestamp = 9000) {
  rtp_rtcp::RtpPacketReceived packet;
  packet.SetSsrc(0x10203040);
  packet.SetPayloadType(102);
  packet.SetSequenceNumber(sequence);
  packet.SetTimestamp(timestamp);
  packet.SetMarker(marker);
  packet.set_arrival_time(base::Timestamp::Millis(100));
  uint8_t* destination = packet.AllocatePayload(payload.size());
  std::copy(payload.begin(), payload.end(), destination);
  return packet;
}

TEST(VideoRtpReceiveStreamTest, ReordersFuAAndEmitsAnnexBIdr) {
  VideoRtpReceiveStream stream({
      .remote_ssrc = 0x10203040,
      .payload_type = 102,
  });
  RecordingVideoSink sink;
  stream.AddOrUpdateSink(&sink, {});
  stream.Start();

  const std::array<uint8_t, 4> first = {0x7c, 0x85, 0x11, 0x22};
  const std::array<uint8_t, 4> middle = {0x7c, 0x05, 0x33, 0x44};
  const std::array<uint8_t, 4> last = {0x7c, 0x45, 0x55, 0x66};
  stream.OnRtpPacket(Packet(11, false, middle));
  stream.OnRtpPacket(Packet(12, true, last));
  EXPECT_EQ(stream.GetStats().frames_delivered, 0U);
  stream.OnRtpPacket(Packet(10, false, first));

  const auto frames = sink.WaitForFrames(1);
  const std::shared_ptr<MediaFrame> frame =
      frames.empty() ? nullptr : frames.front();
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->codec(), CodecId::AVE_CODEC_ID_H264);
  EXPECT_EQ(frame->picture_type(), PictureType::I);
  EXPECT_EQ(frame->pts(), base::Timestamp::Zero());
  EXPECT_EQ(std::vector<uint8_t>(frame->data(), frame->data() + frame->size()),
            (std::vector<uint8_t>{0, 0, 0, 1, 0x65, 0x11, 0x22, 0x33, 0x44,
                                  0x55, 0x66}));
  EXPECT_EQ(stream.GetStats().frames_delivered, 1U);

  stream.Stop();
  stream.RemoveSink(&sink);
}

TEST(VideoRtpReceiveStreamTest, ResetsDiscontinuousTimestampMonotonically) {
  VideoRtpReceiveStream stream({
      .remote_ssrc = 0x10203040,
      .payload_type = 102,
  });
  RecordingVideoSink sink;
  stream.AddOrUpdateSink(&sink, {});
  stream.Start();

  const std::array<uint8_t, 3> idr = {0x65, 0x11, 0x22};
  const std::array<uint8_t, 3> delta = {0x41, 0x33, 0x44};
  stream.OnRtpPacket(Packet(100, true, idr, 3'000'000));
  stream.OnRtpPacket(Packet(101, true, delta, 3'003'000));
  stream.OnRtpPacket(Packet(10, true, idr, 1'000));
  stream.OnRtpPacket(Packet(11, true, delta, 4'000));

  const auto frames = sink.WaitForFrames(4);
  ASSERT_EQ(frames.size(), 4U);
  EXPECT_EQ(frames[0]->pts(), base::Timestamp::Zero());
  EXPECT_GT(frames[1]->pts(), frames[0]->pts());
  EXPECT_GT(frames[2]->pts(), frames[1]->pts());
  EXPECT_GT(frames[3]->pts(), frames[2]->pts());

  stream.Stop();
  stream.RemoveSink(&sink);
}

TEST(VideoRtpReceiveStreamTest, RejectsNestedFuAStartFragments) {
  VideoRtpReceiveStream stream({
      .remote_ssrc = 0x10203040,
      .payload_type = 102,
  });
  RecordingVideoSink sink;
  stream.AddOrUpdateSink(&sink, {});
  stream.Start();

  const std::array<uint8_t, 3> first = {0x7c, 0x85, 0x11};
  const std::array<uint8_t, 3> nested_start = {0x7c, 0x85, 0x22};
  const std::array<uint8_t, 3> last = {0x7c, 0x45, 0x33};
  stream.OnRtpPacket(Packet(10, false, first));
  stream.OnRtpPacket(Packet(11, false, nested_start));
  stream.OnRtpPacket(Packet(12, true, last));

  EXPECT_EQ(stream.GetStats().frames_delivered, 0U);
  EXPECT_EQ(stream.GetStats().invalid_packets_dropped, 1U);

  stream.Stop();
  stream.RemoveSink(&sink);
}

}  // namespace
}  // namespace ave::media::rtp_session
