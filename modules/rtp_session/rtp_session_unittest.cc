#include "media/modules/rtp_session/rtp_session.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "base/clock.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sink_interface.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extensions.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"
#include "media/modules/rtp_session/transport/rtp_transport.h"
#include "test/gtest.h"

namespace ave::media::rtp_session {
namespace {

constexpr uint8_t kMidExtensionId = 1;
constexpr PacketEndpoint kUnusedEndpoint{};

class FakePacketTransport final : public PacketTransport {
 public:
  status_t Start(PacketTransportSink* sink) override {
    if (sink == nullptr) {
      return BAD_VALUE;
    }
    if (sink_ != nullptr) {
      return ALREADY_EXISTS;
    }
    sink_ = sink;
    return OK;
  }

  status_t Stop() override {
    sink_ = nullptr;
    return OK;
  }

  status_t SendPacket(std::span<const uint8_t> /*packet*/,
                      const PacketEndpoint& /*destination*/) override {
    return sink_ == nullptr ? INVALID_OPERATION : OK;
  }

  void Inject(std::span<const uint8_t> packet) {
    if (sink_ != nullptr) {
      sink_->OnPacketReceived(packet, kUnusedEndpoint,
                              base::Timestamp::Millis(123));
    }
  }

 private:
  PacketTransportSink* sink_ = nullptr;
};

class RecordingRtpSink final : public rtp_rtcp::RtpPacketSinkInterface {
 public:
  void OnRtpPacket(const rtp_rtcp::RtpPacketReceived& packet) override {
    ssrcs_.push_back(packet.Ssrc());
    payload_types_.push_back(packet.PayloadType());
  }

  const std::vector<uint32_t>& ssrcs() const { return ssrcs_; }
  const std::vector<uint8_t>& payload_types() const { return payload_types_; }

 private:
  std::vector<uint32_t> ssrcs_;
  std::vector<uint8_t> payload_types_;
};

class RecordingObserver final : public RtpSessionObserver {
 public:
  void OnSessionPacketDiscarded(RtpSessionDiscardReason reason,
                                std::span<const uint8_t> /*packet*/,
                                const PacketEndpoint& /*source*/) override {
    reasons_.push_back(reason);
  }

  const std::vector<RtpSessionDiscardReason>& reasons() const {
    return reasons_;
  }

 private:
  std::vector<RtpSessionDiscardReason> reasons_;
};

struct SessionBundle {
  std::unique_ptr<RtpSession> session{};
  FakePacketTransport* packet_transport = nullptr;
};

SessionBundle CreateSession(
    RecordingObserver* observer = nullptr,
    bool enable_mid = false,
    RtpSession::Config::ReceiveStreamFactory receive_stream_factory = {}) {
  auto packet_transport = std::make_unique<FakePacketTransport>();
  FakePacketTransport* const packet_transport_ptr = packet_transport.get();
  RtpTransport::Config transport_config{
      .packet_transport = std::move(packet_transport),
  };
  RtpSession::Config session_config{
      .transport = std::make_unique<RtpTransport>(std::move(transport_config)),
      .clock = base::Clock::GetRealTimeClock(),
      .mid_extension_id =
          enable_mid ? std::optional<uint8_t>(kMidExtensionId) : std::nullopt,
      .receive_stream_factory = std::move(receive_stream_factory),
      .observer = observer,
  };
  return {
      .session = std::make_unique<RtpSession>(std::move(session_config)),
      .packet_transport = packet_transport_ptr,
  };
}

std::vector<uint8_t> CreateRtpPacket(uint32_t ssrc,
                                     uint8_t payload_type,
                                     uint16_t sequence_number,
                                     std::string_view mid = {}) {
  rtp_rtcp::RtpHeaderExtensionMap extension_map;
  const rtp_rtcp::RtpHeaderExtensionMap* extension_map_ptr = nullptr;
  if (!mid.empty()) {
    EXPECT_TRUE(extension_map.Register<rtp_rtcp::RtpMid>(kMidExtensionId));
    extension_map_ptr = &extension_map;
  }
  rtp_rtcp::RtpPacket packet(extension_map_ptr);
  packet.SetSsrc(ssrc);
  packet.SetPayloadType(payload_type);
  packet.SetSequenceNumber(sequence_number);
  packet.SetTimestamp(static_cast<uint32_t>(sequence_number) * 480);
  if (!mid.empty()) {
    EXPECT_TRUE(packet.SetExtension<rtp_rtcp::RtpMid>(mid));
  }
  constexpr std::array<uint8_t, 4> kPayload = {1, 2, 3, 4};
  uint8_t* const payload = packet.SetPayloadSize(std::size(kPayload));
  std::memcpy(payload, kPayload.data(), kPayload.size());
  return std::vector<uint8_t>(packet.data(), packet.data() + packet.size());
}

RtpReceiveStreamConfig StreamConfig(
    RtpMediaType media_type,
    uint8_t payload_type,
    std::optional<uint32_t> remote_ssrc = std::nullopt) {
  return {
      .media_type = media_type,
      .payload_types = {payload_type},
      .clock_rate_hz = media_type == RtpMediaType::kAudio ? 48000U : 90000U,
      .remote_ssrc = remote_ssrc,
  };
}

TEST(RtpSessionTest, RoutesTwoExplicitAudioSsrcsAndKeepsStatsIndependent) {
  SessionBundle bundle = CreateSession();
  auto first_sink = std::make_shared<RecordingRtpSink>();
  auto second_sink = std::make_shared<RecordingRtpSink>();
  RtpReceiveStreamId first_id = 0;
  RtpReceiveStreamId second_id = 0;
  ASSERT_EQ(
      bundle.session->AddReceiveStream(
          StreamConfig(RtpMediaType::kAudio, 96, 1111), first_sink, &first_id),
      OK);
  ASSERT_EQ(bundle.session->AddReceiveStream(
                StreamConfig(RtpMediaType::kAudio, 96, 2222), second_sink,
                &second_id),
            OK);
  ASSERT_EQ(bundle.session->Start(), OK);

  bundle.packet_transport->Inject(CreateRtpPacket(1111, 96, 1));
  bundle.packet_transport->Inject(CreateRtpPacket(1111, 96, 2));
  bundle.packet_transport->Inject(CreateRtpPacket(2222, 96, 1));
  bundle.packet_transport->Inject(CreateRtpPacket(1111, 97, 3));

  EXPECT_EQ(first_sink->ssrcs(), (std::vector<uint32_t>{1111, 1111}));
  EXPECT_EQ(second_sink->ssrcs(), (std::vector<uint32_t>{2222}));
  const RtpSessionStats stats = bundle.session->GetStats();
  ASSERT_EQ(stats.receive_streams.size(), 2U);
  EXPECT_EQ(stats.receive_streams[0].stream_id, first_id);
  EXPECT_EQ(stats.receive_streams[0].packets_received, 2U);
  EXPECT_EQ(stats.receive_streams[1].stream_id, second_id);
  EXPECT_EQ(stats.receive_streams[1].packets_received, 1U);
  EXPECT_EQ(stats.rtp_packets_received, 3U);
  EXPECT_EQ(stats.unmatched_rtp_packets, 1U);
}

TEST(RtpSessionTest, DiscoversDistinctAudioAndVideoPayloadTypes) {
  SessionBundle bundle = CreateSession();
  auto audio_sink = std::make_shared<RecordingRtpSink>();
  auto video_sink = std::make_shared<RecordingRtpSink>();
  RtpReceiveStreamId audio_id = 0;
  RtpReceiveStreamId video_id = 0;
  ASSERT_EQ(bundle.session->AddReceiveStream(
                StreamConfig(RtpMediaType::kAudio, 96), audio_sink, &audio_id),
            OK);
  ASSERT_EQ(bundle.session->AddReceiveStream(
                StreamConfig(RtpMediaType::kVideo, 97), video_sink, &video_id),
            OK);
  ASSERT_EQ(bundle.session->Start(), OK);

  bundle.packet_transport->Inject(CreateRtpPacket(3001, 96, 1));
  bundle.packet_transport->Inject(CreateRtpPacket(4001, 97, 1));
  bundle.packet_transport->Inject(CreateRtpPacket(3002, 96, 1));

  EXPECT_EQ(audio_sink->payload_types(), (std::vector<uint8_t>{96}));
  EXPECT_EQ(video_sink->payload_types(), (std::vector<uint8_t>{97}));
  const RtpSessionStats stats = bundle.session->GetStats();
  ASSERT_EQ(stats.receive_streams.size(), 2U);
  EXPECT_EQ(stats.receive_streams[0].remote_ssrcs,
            (std::vector<uint32_t>{3001}));
  EXPECT_EQ(stats.receive_streams[1].remote_ssrcs,
            (std::vector<uint32_t>{4001}));
  EXPECT_EQ(stats.unmatched_rtp_packets, 1U);
}

TEST(RtpSessionTest, FactoryCreatesOneStreamPerNewSsrc) {
  std::vector<std::shared_ptr<RecordingRtpSink>> sinks;
  auto factory = [&sinks](const rtp_rtcp::RtpPacketReceived& packet,
                          const PacketEndpoint& /*source*/)
      -> std::optional<DiscoveredRtpReceiveStream> {
    auto sink = std::make_shared<RecordingRtpSink>();
    sinks.push_back(sink);
    return DiscoveredRtpReceiveStream{
        .config = StreamConfig(RtpMediaType::kAudio, packet.PayloadType()),
        .sink = std::move(sink),
    };
  };
  SessionBundle bundle = CreateSession(nullptr, false, std::move(factory));
  ASSERT_EQ(bundle.session->Start(), OK);

  bundle.packet_transport->Inject(CreateRtpPacket(5001, 96, 1));
  bundle.packet_transport->Inject(CreateRtpPacket(5002, 96, 1));
  bundle.packet_transport->Inject(CreateRtpPacket(5001, 96, 2));

  ASSERT_EQ(sinks.size(), 2U);
  EXPECT_EQ(sinks[0]->ssrcs(), (std::vector<uint32_t>{5001, 5001}));
  EXPECT_EQ(sinks[1]->ssrcs(), (std::vector<uint32_t>{5002}));
  const RtpSessionStats stats = bundle.session->GetStats();
  EXPECT_EQ(stats.rtp_packets_received, 3U);
  ASSERT_EQ(stats.receive_streams.size(), 2U);
  EXPECT_EQ(stats.receive_streams[0].packets_received, 2U);
  EXPECT_EQ(stats.receive_streams[1].packets_received, 1U);
}

TEST(RtpSessionTest, RejectsDuplicateSsrcAndDropsAmbiguousPayloadType) {
  RecordingObserver observer;
  SessionBundle bundle = CreateSession(&observer);
  auto first_sink = std::make_shared<RecordingRtpSink>();
  auto second_sink = std::make_shared<RecordingRtpSink>();
  RtpReceiveStreamId stream_id = 0;
  ASSERT_EQ(
      bundle.session->AddReceiveStream(
          StreamConfig(RtpMediaType::kAudio, 96, 5555), first_sink, &stream_id),
      OK);
  EXPECT_EQ(bundle.session->AddReceiveStream(
                StreamConfig(RtpMediaType::kVideo, 97, 5555), second_sink,
                &stream_id),
            ALREADY_EXISTS);
  ASSERT_EQ(bundle.session->AddReceiveStream(
                StreamConfig(RtpMediaType::kAudio, 98), first_sink, &stream_id),
            OK);
  ASSERT_EQ(
      bundle.session->AddReceiveStream(StreamConfig(RtpMediaType::kVideo, 98),
                                       second_sink, &stream_id),
      OK);
  ASSERT_EQ(bundle.session->Start(), OK);

  bundle.packet_transport->Inject(CreateRtpPacket(9999, 98, 1));

  EXPECT_TRUE(first_sink->ssrcs().empty());
  EXPECT_TRUE(second_sink->ssrcs().empty());
  ASSERT_EQ(observer.reasons().size(), 1U);
  EXPECT_EQ(observer.reasons()[0],
            RtpSessionDiscardReason::kUnmatchedRtpPacket);
  EXPECT_EQ(bundle.session->GetStats().unmatched_rtp_packets, 1U);
}

TEST(RtpSessionTest, MidRouteHasPriorityOverRegisteredSsrc) {
  SessionBundle bundle = CreateSession(nullptr, true);
  auto mid_sink = std::make_shared<RecordingRtpSink>();
  auto ssrc_sink = std::make_shared<RecordingRtpSink>();
  RtpReceiveStreamId stream_id = 0;
  RtpReceiveStreamConfig mid_config =
      StreamConfig(RtpMediaType::kAudio, 96, 7001);
  mid_config.mid = "audio";
  ASSERT_EQ(bundle.session->AddReceiveStream(std::move(mid_config), mid_sink,
                                             &stream_id),
            OK);
  ASSERT_EQ(
      bundle.session->AddReceiveStream(
          StreamConfig(RtpMediaType::kVideo, 97, 7002), ssrc_sink, &stream_id),
      OK);
  ASSERT_EQ(bundle.session->Start(), OK);

  bundle.packet_transport->Inject(CreateRtpPacket(7002, 96, 1, "audio"));

  EXPECT_EQ(mid_sink->ssrcs(), (std::vector<uint32_t>{7002}));
  EXPECT_TRUE(ssrc_sink->ssrcs().empty());
}

TEST(RtpSessionTest, ClassifiesRtcpMuxAndRejectsMalformedPackets) {
  RecordingObserver observer;
  SessionBundle bundle = CreateSession(&observer);
  ASSERT_EQ(bundle.session->Start(), OK);
  constexpr std::array<uint8_t, 8> kReceiverReport = {0x80, 201, 0, 1,
                                                      0,    0,   0, 1};
  constexpr std::array<uint8_t, 8> kMalformedRtcp = {0x80, 201, 0, 2,
                                                     0,    0,   0, 1};
  constexpr std::array<uint8_t, 3> kMalformedRtp = {0x80, 96, 0};

  bundle.packet_transport->Inject(kReceiverReport);
  bundle.packet_transport->Inject(kMalformedRtcp);
  bundle.packet_transport->Inject(kMalformedRtp);

  const RtpSessionStats stats = bundle.session->GetStats();
  EXPECT_EQ(stats.rtcp_packets_received, 1U);
  EXPECT_EQ(stats.invalid_packets, 2U);
  EXPECT_EQ(observer.reasons().size(), 2U);
  EXPECT_EQ(observer.reasons()[0], RtpSessionDiscardReason::kInvalidRtcpPacket);
  EXPECT_EQ(observer.reasons()[1], RtpSessionDiscardReason::kInvalidRtpPacket);
}

TEST(RtpSessionTest, ControlsLifecycleAndStreamRemoval) {
  SessionBundle bundle = CreateSession();
  auto sink = std::make_shared<RecordingRtpSink>();
  RtpReceiveStreamId stream_id = 0;
  ASSERT_EQ(bundle.session->AddReceiveStream(
                StreamConfig(RtpMediaType::kAudio, 96, 8001), sink, &stream_id),
            OK);
  EXPECT_EQ(bundle.session->Start(), OK);
  EXPECT_TRUE(bundle.session->started());
  EXPECT_EQ(bundle.session->Start(), ALREADY_EXISTS);
  EXPECT_EQ(bundle.session->RemoveReceiveStream(stream_id), OK);
  EXPECT_EQ(bundle.session->RemoveReceiveStream(stream_id), NAME_NOT_FOUND);
  EXPECT_EQ(bundle.session->Stop(), OK);
  EXPECT_FALSE(bundle.session->started());
  EXPECT_EQ(bundle.session->Stop(), OK);
}

}  // namespace
}  // namespace ave::media::rtp_session
