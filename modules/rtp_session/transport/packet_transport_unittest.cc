#include "media/modules/rtp_session/transport/udp_packet_transport.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

#include "base/clock.h"
#include "media/modules/rtp_session/transport/packet_transport_sink.h"
#include "media/modules/rtp_session/transport/rtp_transport.h"
#include "test/gtest.h"

namespace ave::media::rtp_session {
namespace {

uint16_t ReserveLoopbackPort() {
  const int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    return 0;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&address),
           sizeof(address)) != 0) {
    close(socket_fd);
    return 0;
  }

  socklen_t address_size = sizeof(address);
  if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address),
                  &address_size) != 0) {
    close(socket_fd);
    return 0;
  }
  close(socket_fd);
  return ntohs(address.sin_port);
}

PacketEndpoint LoopbackEndpoint(uint16_t port) {
  return *PacketEndpoint::CreateIpv4("127.0.0.1", port);
}

class RecordingSink final : public PacketTransportSink {
 public:
  void OnPacketReceived(std::span<const uint8_t> packet,
                        const PacketEndpoint& source,
                        base::Timestamp arrival_time) override {
    std::scoped_lock lock(mutex_);
    packets_.emplace_back(packet.begin(), packet.end());
    source_ = source;
    arrival_time_ = arrival_time;
    condition_.notify_all();
  }

  bool WaitForPacketCount(size_t count) {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(2), [this, count] {
      return packets_.size() >= count;
    });
  }

  std::vector<uint8_t> packet(size_t index) const {
    std::scoped_lock lock(mutex_);
    return packets_.at(index);
  }

  PacketEndpoint source() const {
    std::scoped_lock lock(mutex_);
    return source_;
  }

  base::Timestamp arrival_time() const {
    std::scoped_lock lock(mutex_);
    return arrival_time_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::vector<std::vector<uint8_t>> packets_;
  PacketEndpoint source_;
  base::Timestamp arrival_time_ = base::Timestamp::Zero();
};

class BlockingSink final : public PacketTransportSink {
 public:
  void OnPacketReceived(std::span<const uint8_t> /*packet*/,
                        const PacketEndpoint& /*source*/,
                        base::Timestamp /*arrival_time*/) override {
    std::unique_lock<std::mutex> lock(mutex_);
    callback_entered_ = true;
    condition_.notify_all();
    condition_.wait(lock, [this] { return callback_released_; });
  }

  bool WaitUntilCallbackEntered() {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(2),
                               [this] { return callback_entered_; });
  }

  void ReleaseCallback() {
    std::scoped_lock lock(mutex_);
    callback_released_ = true;
    condition_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  bool callback_entered_ = false;
  bool callback_released_ = false;
};

std::unique_ptr<UdpPacketTransport> CreateUdpTransport(uint16_t local_port) {
  UdpPacketTransport::Config config{
      .local_endpoint = LoopbackEndpoint(local_port),
      .clock = base::Clock::GetRealTimeClock(),
  };
  return std::make_unique<UdpPacketTransport>(std::move(config));
}

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

  status_t SendPacket(std::span<const uint8_t> packet,
                      const PacketEndpoint& destination) override {
    if (sink_ == nullptr) {
      return INVALID_OPERATION;
    }
    packets_.emplace_back(packet.begin(), packet.end());
    destinations_.push_back(destination);
    return OK;
  }

  void InjectPacket(std::span<const uint8_t> packet,
                    const PacketEndpoint& source,
                    base::Timestamp arrival_time) {
    if (sink_ != nullptr) {
      sink_->OnPacketReceived(packet, source, arrival_time);
    }
  }

  const std::vector<PacketEndpoint>& destinations() const {
    return destinations_;
  }

  const std::vector<uint8_t>& packet(size_t index) const {
    return packets_.at(index);
  }

 private:
  PacketTransportSink* sink_ = nullptr;
  std::vector<std::vector<uint8_t>> packets_;
  std::vector<PacketEndpoint> destinations_;
};

class RecordingTransportObserver final : public RtpTransportObserver {
 public:
  void OnPacketSent(RtpTransportPacketType packet_type,
                    size_t /*bytes*/,
                    int64_t send_duration_us,
                    status_t status) override {
    EXPECT_GE(send_duration_us, 0);
    if (packet_type == RtpTransportPacketType::kRtp) {
      ++rtp_packets_;
    } else {
      ++rtcp_packets_;
    }
    if (status != OK) {
      ++failures_;
    }
  }

  void OnPacketDiscarded(RtpTransportPacketType /*packet_type*/,
                         status_t status) override {
    ++discarded_packets_;
    last_discard_status_ = status;
  }

  uint64_t rtp_packets() const { return rtp_packets_; }
  uint64_t rtcp_packets() const { return rtcp_packets_; }
  uint64_t failures() const { return failures_; }
  uint64_t discarded_packets() const { return discarded_packets_; }
  status_t last_discard_status() const { return last_discard_status_; }

 private:
  uint64_t rtp_packets_ = 0;
  uint64_t rtcp_packets_ = 0;
  uint64_t failures_ = 0;
  uint64_t discarded_packets_ = 0;
  status_t last_discard_status_ = OK;
};

SrtpKey MakeTransportTestKey(uint8_t seed) {
  SrtpKey key;
  for (size_t index = 0; index < key.bytes.size(); ++index) {
    key.bytes[index] = static_cast<uint8_t>(seed + index);
  }
  return key;
}

TEST(PacketEndpointTest, ParsesAndFormatsIpv4Address) {
  const auto endpoint = PacketEndpoint::CreateIpv4("192.168.1.20", 5000);
  ASSERT_TRUE(endpoint.has_value());
  EXPECT_TRUE(endpoint->IsValid());
  EXPECT_FALSE(endpoint->IsUnspecified());
  EXPECT_EQ(endpoint->AddressString(), "192.168.1.20");
  EXPECT_EQ(endpoint->port(), 5000u);

  EXPECT_FALSE(PacketEndpoint::CreateIpv4("not-an-address", 5000).has_value());
  EXPECT_FALSE(PacketEndpoint::CreateIpv4("127.0.0.1", 0).has_value());
}

TEST(UdpPacketTransportTest, SendsLoopbackPayloadBoundaries) {
  const uint16_t receiver_port = ReserveLoopbackPort();
  const uint16_t sender_port = ReserveLoopbackPort();
  ASSERT_NE(receiver_port, 0u);
  ASSERT_NE(sender_port, 0u);
  ASSERT_NE(receiver_port, sender_port);

  auto receiver = CreateUdpTransport(receiver_port);
  auto sender = CreateUdpTransport(sender_port);
  RecordingSink receiver_sink;
  RecordingSink sender_sink;
  ASSERT_EQ(receiver->Start(&receiver_sink), OK);
  ASSERT_EQ(sender->Start(&sender_sink), OK);

  const std::vector<uint8_t> small_packet{0x01};
  std::vector<uint8_t> maximum_packet(PacketTransport::kMaximumUdpPayloadSize,
                                      0x5a);
  ASSERT_EQ(sender->SendPacket(small_packet, LoopbackEndpoint(receiver_port)),
            OK);
  ASSERT_EQ(sender->SendPacket(maximum_packet, LoopbackEndpoint(receiver_port)),
            OK);
  ASSERT_TRUE(receiver_sink.WaitForPacketCount(2));
  EXPECT_EQ(receiver_sink.packet(0), small_packet);
  EXPECT_EQ(receiver_sink.packet(1), maximum_packet);
  EXPECT_EQ(receiver_sink.source(), LoopbackEndpoint(sender_port));
  EXPECT_GT(receiver_sink.arrival_time(), base::Timestamp::Zero());

  EXPECT_EQ(sender->SendPacket({}, LoopbackEndpoint(receiver_port)), BAD_VALUE);
  std::vector<uint8_t> oversized_packet(
      PacketTransport::kMaximumUdpPayloadSize + 1, 0x00);
  EXPECT_EQ(
      sender->SendPacket(oversized_packet, LoopbackEndpoint(receiver_port)),
      BAD_VALUE);
  EXPECT_EQ(sender->Stop(), OK);
  EXPECT_EQ(receiver->Stop(), OK);
}

TEST(UdpPacketTransportTest, SupportsStartStopAndRestart) {
  const uint16_t receiver_port = ReserveLoopbackPort();
  const uint16_t sender_port = ReserveLoopbackPort();
  ASSERT_NE(receiver_port, 0u);
  ASSERT_NE(sender_port, 0u);

  auto receiver = CreateUdpTransport(receiver_port);
  auto sender = CreateUdpTransport(sender_port);
  RecordingSink first_receiver_sink;
  RecordingSink second_receiver_sink;
  RecordingSink sender_sink;

  ASSERT_EQ(receiver->Start(&first_receiver_sink), OK);
  EXPECT_EQ(receiver->Start(&first_receiver_sink), ALREADY_EXISTS);
  ASSERT_EQ(receiver->Stop(), OK);
  ASSERT_EQ(receiver->Start(&second_receiver_sink), OK);
  ASSERT_EQ(sender->Start(&sender_sink), OK);

  const std::vector<uint8_t> packet{0x10, 0x20, 0x30};
  ASSERT_EQ(sender->SendPacket(packet, LoopbackEndpoint(receiver_port)), OK);
  ASSERT_TRUE(second_receiver_sink.WaitForPacketCount(1));
  EXPECT_EQ(second_receiver_sink.packet(0), packet);
  EXPECT_EQ(sender->Stop(), OK);
  EXPECT_EQ(receiver->Stop(), OK);
  EXPECT_EQ(receiver->Stop(), OK);
}

TEST(UdpPacketTransportTest, StopWakesBlockedReceiveThread) {
  const uint16_t local_port = ReserveLoopbackPort();
  ASSERT_NE(local_port, 0u);
  auto transport = CreateUdpTransport(local_port);
  RecordingSink sink;
  ASSERT_EQ(transport->Start(&sink), OK);

  const auto start_time = std::chrono::steady_clock::now();
  EXPECT_EQ(transport->Stop(), OK);
  const auto stop_duration = std::chrono::steady_clock::now() - start_time;
  EXPECT_LT(stop_duration, std::chrono::milliseconds(500));
}

TEST(UdpPacketTransportTest, StopWaitsForActiveCallbackAndThenCompletes) {
  const uint16_t receiver_port = ReserveLoopbackPort();
  const uint16_t sender_port = ReserveLoopbackPort();
  ASSERT_NE(receiver_port, 0u);
  ASSERT_NE(sender_port, 0u);

  auto receiver = CreateUdpTransport(receiver_port);
  auto sender = CreateUdpTransport(sender_port);
  BlockingSink receiver_sink;
  RecordingSink sender_sink;
  ASSERT_EQ(receiver->Start(&receiver_sink), OK);
  ASSERT_EQ(sender->Start(&sender_sink), OK);

  const std::vector<uint8_t> packet{0x01, 0x02};
  ASSERT_EQ(sender->SendPacket(packet, LoopbackEndpoint(receiver_port)), OK);
  ASSERT_TRUE(receiver_sink.WaitUntilCallbackEntered());

  auto stop_result =
      std::async(std::launch::async, [&receiver] { return receiver->Stop(); });
  EXPECT_EQ(stop_result.wait_for(std::chrono::milliseconds(20)),
            std::future_status::timeout);
  receiver_sink.ReleaseCallback();
  ASSERT_EQ(stop_result.wait_for(std::chrono::seconds(2)),
            std::future_status::ready);
  EXPECT_EQ(stop_result.get(), OK);
  EXPECT_EQ(sender->Stop(), OK);
}

TEST(RtpTransportTest, UpdatesEndpointsAndForwardsArrivalTime) {
  const PacketEndpoint first_endpoint = LoopbackEndpoint(5000);
  const PacketEndpoint second_endpoint = LoopbackEndpoint(5002);
  const PacketEndpoint third_endpoint = LoopbackEndpoint(5004);
  auto fake_packet_transport = std::make_unique<FakePacketTransport>();
  FakePacketTransport* const fake = fake_packet_transport.get();
  RecordingTransportObserver observer;
  RtpTransport::Config config{
      .packet_transport = std::move(fake_packet_transport),
      .remote_endpoint = first_endpoint,
      .observer = &observer,
  };
  RtpTransport transport(std::move(config));

  std::vector<uint8_t> received_packet;
  PacketEndpoint received_source;
  base::Timestamp received_arrival = base::Timestamp::Zero();
  ASSERT_EQ(transport.Start([&](std::span<const uint8_t> packet,
                                const PacketEndpoint& source,
                                base::Timestamp arrival_time) {
    received_packet.assign(packet.begin(), packet.end());
    received_source = source;
    received_arrival = arrival_time;
  }),
            OK);

  const std::vector<uint8_t> rtp_packet{0x80, 0x60, 0x00, 0x01};
  EXPECT_TRUE(transport.SendRtp(rtp_packet, rtp_rtcp::PacketOptions{}));
  ASSERT_EQ(fake->destinations().size(), 1u);
  EXPECT_EQ(fake->destinations()[0], first_endpoint);
  EXPECT_EQ(fake->packet(0), rtp_packet);

  ASSERT_EQ(transport.SetRemoteEndpoint(second_endpoint), OK);
  EXPECT_TRUE(transport.SendRtp(rtp_packet, rtp_rtcp::PacketOptions{}));
  ASSERT_EQ(fake->destinations().size(), 2u);
  EXPECT_EQ(fake->destinations()[1], second_endpoint);

  ASSERT_EQ(transport.AddRemoteRtcpEndpoint(third_endpoint), OK);
  ASSERT_EQ(transport.AddRemoteRtcpEndpoint(third_endpoint), OK);
  const std::vector<uint8_t> rtcp_packet{0x80, 0xc8, 0x00, 0x01};
  EXPECT_TRUE(transport.SendRtcp(rtcp_packet));
  ASSERT_EQ(fake->destinations().size(), 4u);
  EXPECT_EQ(fake->destinations()[2], second_endpoint);
  EXPECT_EQ(fake->destinations()[3], third_endpoint);

  const base::Timestamp arrival_time = base::Timestamp::Millis(1234);
  fake->InjectPacket(rtp_packet, second_endpoint, arrival_time);
  EXPECT_EQ(received_packet, rtp_packet);
  EXPECT_EQ(received_source, second_endpoint);
  EXPECT_EQ(received_arrival, arrival_time);
  EXPECT_EQ(observer.rtp_packets(), 2u);
  EXPECT_EQ(observer.rtcp_packets(), 2u);
  EXPECT_EQ(observer.failures(), 0u);
  EXPECT_EQ(transport.Stop(), OK);
}

TEST(RtpTransportTest, ProtectsAndUnprotectsRtpAndRtcp) {
  const PacketEndpoint sender_endpoint = LoopbackEndpoint(5010);
  const PacketEndpoint receiver_endpoint = LoopbackEndpoint(5012);
  const SrtpKey sender_key = MakeTransportTestKey(0x10);
  const SrtpKey receiver_key = MakeTransportTestKey(0x70);

  auto sender_packet_transport = std::make_unique<FakePacketTransport>();
  FakePacketTransport* const sender_fake = sender_packet_transport.get();
  RtpTransport::Config sender_config{
      .packet_transport = std::move(sender_packet_transport),
      .remote_endpoint = receiver_endpoint,
      .security =
          {
              .mode = RtpSecurityMode::kSrtpPreSharedKey,
              .outbound_key = sender_key,
              .inbound_key = receiver_key,
          },
  };
  RtpTransport sender(std::move(sender_config));

  auto receiver_packet_transport = std::make_unique<FakePacketTransport>();
  FakePacketTransport* const receiver_fake = receiver_packet_transport.get();
  RecordingTransportObserver receiver_observer;
  RtpTransport::Config receiver_config{
      .packet_transport = std::move(receiver_packet_transport),
      .remote_endpoint = sender_endpoint,
      .security =
          {
              .mode = RtpSecurityMode::kSrtpPreSharedKey,
              .outbound_key = receiver_key,
              .inbound_key = sender_key,
          },
      .observer = &receiver_observer,
  };
  RtpTransport receiver(std::move(receiver_config));

  std::vector<std::vector<uint8_t>> received_packets;
  ASSERT_EQ(sender.Start([](std::span<const uint8_t> /*packet*/,
                            const PacketEndpoint& /*source*/,
                            base::Timestamp /*arrival_time*/) {}),
            OK);
  ASSERT_EQ(receiver.Start([&](std::span<const uint8_t> packet,
                               const PacketEndpoint& /*source*/,
                               base::Timestamp /*arrival_time*/) {
    received_packets.emplace_back(packet.begin(), packet.end());
  }),
            OK);

  const std::vector<uint8_t> rtp_packet{
      0x80, 0x60, 0x00, 0x01, 0x00, 0x00, 0x01, 0xe0,
      0x11, 0x22, 0x33, 0x44, 0x4f, 0x70, 0x75, 0x73,
  };
  ASSERT_TRUE(sender.SendRtp(rtp_packet, rtp_rtcp::PacketOptions{}));
  ASSERT_EQ(sender_fake->destinations().size(), 1u);
  ASSERT_GT(sender_fake->packet(0).size(), rtp_packet.size());
  EXPECT_NE(sender_fake->packet(0), rtp_packet);
  receiver_fake->InjectPacket(sender_fake->packet(0), sender_endpoint,
                              base::Timestamp::Millis(10));
  ASSERT_EQ(received_packets.size(), 1u);
  EXPECT_EQ(received_packets[0], rtp_packet);

  const std::vector<uint8_t> rtcp_packet{
      0x80, 0xc8, 0x00, 0x01, 0x11, 0x22, 0x33, 0x44,
  };
  ASSERT_TRUE(sender.SendRtcp(rtcp_packet));
  ASSERT_EQ(sender_fake->destinations().size(), 2u);
  ASSERT_GT(sender_fake->packet(1).size(), rtcp_packet.size());
  receiver_fake->InjectPacket(sender_fake->packet(1), sender_endpoint,
                              base::Timestamp::Millis(20));
  ASSERT_EQ(received_packets.size(), 2u);
  EXPECT_EQ(received_packets[1], rtcp_packet);

  receiver_fake->InjectPacket(sender_fake->packet(0), sender_endpoint,
                              base::Timestamp::Millis(30));
  EXPECT_EQ(received_packets.size(), 2u);
  EXPECT_EQ(receiver_observer.discarded_packets(), 1u);
  EXPECT_EQ(receiver_observer.last_discard_status(), ALREADY_EXISTS);
  EXPECT_EQ(sender.Stop(), OK);
  EXPECT_EQ(receiver.Stop(), OK);
}

}  // namespace
}  // namespace ave::media::rtp_session
