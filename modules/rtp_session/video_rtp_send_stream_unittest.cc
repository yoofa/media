/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/video_rtp_send_stream.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "base/clock.h"
#include "base/task_util/default_task_runner_factory.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"
#include "media/modules/rtp_rtcp/src/transport/transport.h"
#include "media/modules/rtp_rtcp/src/util/rtp_rtcp_impl2.h"
#include "test/gtest.h"

namespace ave::media::rtp_session {
namespace {

class CapturingTransport final : public rtp_rtcp::Transport {
 public:
  ~CapturingTransport() override = default;

  bool SendRtp(std::span<const uint8_t> packet,
               const rtp_rtcp::PacketOptions& options) override {
    static_cast<void>(options);
    std::scoped_lock lock(mutex_);
    rtp_packets_.emplace_back(packet.begin(), packet.end());
    condition_.notify_all();
    return true;
  }

  bool SendRtcp(std::span<const uint8_t> packet) override {
    static_cast<void>(packet);
    return true;
  }

  std::vector<std::vector<uint8_t>> WaitForRtpPackets(size_t minimum_count) {
    std::unique_lock lock(mutex_);
    condition_.wait_for(lock, std::chrono::seconds(2),
                        [&]() { return rtp_packets_.size() >= minimum_count; });
    return rtp_packets_;
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::vector<std::vector<uint8_t>> rtp_packets_;
};

TEST(VideoRtpSendStreamTest, PacketizesLargeAnnexBIdrAsH264FuA) {
  base::Clock* const clock = base::Clock::GetRealTimeClock();
  std::unique_ptr<base::TaskRunnerFactory> task_runner_factory =
      base::CreateDefaultTaskRunnerFactory();
  auto worker_queue = task_runner_factory->CreateTaskRunner(
      "VideoRtpSendStreamTest", base::TaskRunnerFactory::Priority::NORMAL);
  CapturingTransport transport;

  rtp_rtcp::RtpRtcpInterface::Configuration module_config;
  module_config.audio = false;
  module_config.outgoing_transport = &transport;
  module_config.local_media_ssrc = 0x12345678;
  rtp_rtcp::ModuleRtpRtcpImpl2 module(clock, worker_queue.get(), module_config);
  module.SetSendingStatus(true);
  module.SetSendingMediaStatus(true);

  VideoRtpSendStream stream({
      .payload_type = 102,
      .rtp_clock_rate_hz = 90000,
      .packetization_mode = H264PacketizationMode::NonInterleaved,
      .rtp_rtcp = &module,
      .clock = clock,
  });
  ASSERT_TRUE(stream.valid());

  constexpr size_t kNaluPayloadSize = 4000;
  auto frame =
      MediaFrame::CreateShared(4 + 1 + kNaluPayloadSize, MediaType::VIDEO);
  uint8_t* const data = frame->data();
  data[0] = 0;
  data[1] = 0;
  data[2] = 0;
  data[3] = 1;
  data[4] = 0x65;
  std::fill(data + 5, data + frame->size(), 0x55);
  frame->SetCodec(CodecId::AVE_CODEC_ID_H264)
      .SetWidth(1920)
      .SetHeight(1080)
      .SetPictureType(PictureType::I)
      .SetPts(base::Timestamp::Millis(100))
      .SetDts(base::Timestamp::Millis(100));
  stream.OnFrame(frame);

  const std::vector<std::vector<uint8_t>> packets =
      transport.WaitForRtpPackets(3);
  ASSERT_GE(packets.size(), 3U);
  std::optional<uint32_t> timestamp;
  std::optional<uint16_t> previous_sequence;
  for (size_t index = 0; index < packets.size(); ++index) {
    rtp_rtcp::RtpPacketReceived packet;
    ASSERT_TRUE(packet.Parse(packets[index]));
    EXPECT_EQ(packet.PayloadType(), 102);
    EXPECT_EQ(packet.Ssrc(), 0x12345678U);
    ASSERT_FALSE(packet.payload().empty());
    EXPECT_EQ(packet.payload()[0] & 0x1f, 28);
    EXPECT_EQ(packet.Marker(), index + 1 == packets.size());
    if (!timestamp.has_value()) {
      timestamp = packet.Timestamp();
    }
    EXPECT_EQ(packet.Timestamp(), *timestamp);
    if (previous_sequence.has_value()) {
      EXPECT_EQ(packet.SequenceNumber(),
                static_cast<uint16_t>(*previous_sequence + 1));
    }
    previous_sequence = packet.SequenceNumber();
  }
}

}  // namespace
}  // namespace ave::media::rtp_session
