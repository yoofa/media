/*
 * rtp_rtcp_socket_demo.cc - RTP/RTCP UDP loopback demo
 *
 * Demonstrates sending and receiving RTP/RTCP packets using rtp_rtcp module
 * and base socket APIs.
 */

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <memory>
#include <span>
#include "base/clock.h"
#include "base/net/async_udp_socket.h"
#include "base/net/socket_address.h"
#include "base/net/socket_thread.h"

#include "base/task_util/default_task_runner_factory.h"
#include "base/task_util/task_runner_base.h"
#include "base/task_util/to_task.h"
#include "media/modules/rtp_rtcp/api/receive_statistics.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/util/rtp_rtcp_impl2.h"
#include "media/modules/rtp_rtcp/src/util/rtp_util.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
// Demo stub until FrameTransformerInterface is ported.
class FrameTransformerInterface {
 public:
  virtual ~FrameTransformerInterface() = default;
};
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"

namespace {

using ave::base::Clock;
using ave::base::TaskRunnerBase;
using ave::base::net::AsyncPacketSocket;
using ave::base::net::AsyncUDPSocket;
using ave::base::net::SocketAddress;
using ave::base::net::SocketThread;
using ave::media::rtp_rtcp::IsRtcpPacket;
using ave::media::rtp_rtcp::IsRtpPacket;
using ave::media::rtp_rtcp::kVideoPayloadTypeFrequency;
using ave::media::rtp_rtcp::ModuleRtpRtcpImpl2;
using ave::media::rtp_rtcp::PacedPacketInfo;
using ave::media::rtp_rtcp::PacketOptions;
using ave::media::rtp_rtcp::ReceiveStatistics;
using ave::media::rtp_rtcp::RtcpMode;
using ave::media::rtp_rtcp::RTCPPacketType;
using ave::media::rtp_rtcp::RtpPacketMediaType;
using ave::media::rtp_rtcp::RtpPacketReceived;
using ave::media::rtp_rtcp::RtpPacketToSend;
using ave::media::rtp_rtcp::RtpRtcpInterface;
using ave::media::rtp_rtcp::Transport;
using std::span;

constexpr int kPayloadType = 96;
constexpr uint32_t kTimestampStep = kVideoPayloadTypeFrequency / 30;
constexpr uint16_t kRtpPortA = 5000;
constexpr uint16_t kRtcpPortA = 5001;
constexpr uint16_t kRtpPortB = 5002;
constexpr uint16_t kRtcpPortB = 5003;
constexpr uint32_t kSsrcA = 0x11111111;
constexpr uint32_t kSsrcB = 0x22222222;

class UdpTransport final : public Transport {
 public:
  explicit UdpTransport(SocketThread* thread) : thread_(thread) {}

  void SetSockets(AsyncUDPSocket* rtp_socket, AsyncUDPSocket* rtcp_socket) {
    rtp_socket_ = rtp_socket;
    rtcp_socket_ = rtcp_socket;
  }

  void SetRemoteAddresses(const SocketAddress& rtp_addr,
                          const SocketAddress& rtcp_addr) {
    remote_rtp_addr_ = rtp_addr;
    remote_rtcp_addr_ = rtcp_addr;
  }

  bool SendRtp(std::span<const uint8_t> packet,
               const PacketOptions& /* options */) override {
    return SendPacket(rtp_socket_, remote_rtp_addr_, packet);
  }

  bool SendRtcp(std::span<const uint8_t> packet) override {
    return SendPacket(rtcp_socket_, remote_rtcp_addr_, packet);
  }

 private:
  bool SendPacket(AsyncUDPSocket* socket,
                  const SocketAddress& addr,
                  std::span<const uint8_t> packet) {
    if (!socket) {
      return false;
    }
    bool sent = false;
    const uint8_t* data = packet.data();
    size_t size = packet.size();
    thread_->Invoke([&]() {
      if (!socket) {
        return;
      }
      sent = socket->SendTo(data, size, addr) >= 0;
    });
    return sent;
  }

  SocketThread* const thread_;
  AsyncUDPSocket* rtp_socket_ = nullptr;
  AsyncUDPSocket* rtcp_socket_ = nullptr;
  SocketAddress remote_rtp_addr_;
  SocketAddress remote_rtcp_addr_;
};

class DemoEndpoint final : public sigslot::has_slots<> {
 public:
  DemoEndpoint(const std::string& name,
               Clock* clock,
               TaskRunnerBase* worker_queue,
               SocketThread* network_thread,
               uint16_t rtp_port,
               uint16_t rtcp_port,
               const SocketAddress& remote_rtp_addr,
               const SocketAddress& remote_rtcp_addr,
               uint32_t local_ssrc,
               uint32_t remote_ssrc)
      : name_(name),
        clock_(clock),
        worker_queue_(worker_queue),
        network_thread_(network_thread),
        transport_(network_thread),
        local_ssrc_(local_ssrc),
        remote_ssrc_(remote_ssrc),
        rtp_port_(rtp_port),
        rtcp_port_(rtcp_port),
        remote_rtp_addr_(remote_rtp_addr),
        remote_rtcp_addr_(remote_rtcp_addr),
        next_timestamp_(0) {}

  void Initialize() {
    network_thread_->Invoke([this]() {
      ave::base::net::Socket* rtp_socket =
          network_thread_->CreateSocket(AF_INET, SOCK_DGRAM);
      ave::base::net::Socket* rtcp_socket =
          network_thread_->CreateSocket(AF_INET, SOCK_DGRAM);
      SocketAddress rtp_bind_addr("127.0.0.1", rtp_port_);
      SocketAddress rtcp_bind_addr("127.0.0.1", rtcp_port_);
      rtp_socket_ = AsyncUDPSocket::Create(rtp_socket, rtp_bind_addr);
      rtcp_socket_ = AsyncUDPSocket::Create(rtcp_socket, rtcp_bind_addr);
      if (rtp_socket_) {
        rtp_socket_->SignalReadPacket.connect(this, &DemoEndpoint::OnPacket);
      }
      if (rtcp_socket_) {
        rtcp_socket_->SignalReadPacket.connect(this, &DemoEndpoint::OnPacket);
      }
    });

    transport_.SetSockets(rtp_socket_, rtcp_socket_);
    transport_.SetRemoteAddresses(remote_rtp_addr_, remote_rtcp_addr_);

    receive_statistics_ = ReceiveStatistics::Create(clock_);
    RtpRtcpInterface::Configuration config;
    config.audio = false;
    config.receiver_only = false;
    config.receive_statistics = receive_statistics_.get();
    config.outgoing_transport = &transport_;
    config.local_media_ssrc = local_ssrc_;
    config.task_runner = worker_queue_;
    module_ =
        std::make_unique<ModuleRtpRtcpImpl2>(clock_, worker_queue_, config);

    worker_queue_->PostDelayedTaskAndWait(
        ave::base::toTask([this]() { ConfigureModule(); }), 0, true);
  }

  void SendRtpMessage(const std::string& message) {
    worker_queue_->PostTask(ave::base::toTask([this, message]() {
      auto packet = std::make_unique<RtpPacketToSend>(nullptr);
      packet->set_packet_type(RtpPacketMediaType::kVideo);
      packet->SetPayloadType(kPayloadType);
      packet->SetSsrc(local_ssrc_);
      packet->SetTimestamp(next_timestamp_);
      packet->SetMarker(true);
      packet->set_first_packet_of_frame(true);
      packet->set_is_key_frame(true);
      packet->set_capture_time(clock_->CurrentTime());
      uint8_t* payload = packet->AllocatePayload(message.size());
      if (!message.empty()) {
        memcpy(payload, message.data(), message.size());
      }

      module_->TrySendPacket(std::move(packet), PacedPacketInfo());
      module_->OnSendingRtpFrame(next_timestamp_, clock_->CurrentTime().ms(),
                                 kPayloadType, false);
      printf("[%s] RTP sent: %s\n", name_.c_str(), message.c_str());
      next_timestamp_ += kTimestampStep;
    }));
  }

  void SendRtcpReport() {
    worker_queue_->PostTask(ave::base::toTask([this]() {
      module_->SendRTCP(RTCPPacketType::kRtcpReport);
      printf("[%s] RTCP report sent\n", name_.c_str());
    }));
  }

  void Stop() {
    network_thread_->Invoke([this]() {
      delete rtp_socket_;
      delete rtcp_socket_;
      rtp_socket_ = nullptr;
      rtcp_socket_ = nullptr;
    });
  }

 private:
  void ConfigureModule() {
    module_->RegisterSendPayloadFrequency(kPayloadType,
                                          kVideoPayloadTypeFrequency);
    module_->SetRTCPStatus(RtcpMode::kCompound);
    module_->SetSendingStatus(true);
    module_->SetSendingMediaStatus(true);
    module_->SetLocalSsrc(local_ssrc_);
    module_->SetRemoteSSRC(remote_ssrc_);
    module_->SetCNAME(name_);
  }

  void OnPacket(AsyncPacketSocket* /* socket */,
                const uint8_t* data,
                size_t size,
                const SocketAddress& /* addr */,
                int64_t /* timestamp */) {
    std::vector<uint8_t> packet(data, data + size);
    worker_queue_->PostTask(ave::base::toTask([this, packet = std::move(
                                                         packet)]() mutable {
      std::span<const uint8_t> view(packet.data(), packet.size());
      if (IsRtcpPacket(view)) {
        module_->IncomingRtcpPacket(view);
        printf("[%s] RTCP received (%zu bytes)\n", name_.c_str(), view.size());
        return;
      }
      if (!IsRtpPacket(view)) {
        printf("[%s] Unknown packet (%zu bytes)\n", name_.c_str(), view.size());
        return;
      }

      RtpPacketReceived rtp_packet;
      if (!rtp_packet.Parse(view)) {
        printf("[%s] Failed to parse RTP packet\n", name_.c_str());
        return;
      }

      rtp_packet.set_arrival_time(clock_->CurrentTime());
      rtp_packet.set_payload_type_frequency(kVideoPayloadTypeFrequency);
      receive_statistics_->OnRtpPacket(rtp_packet);

      auto payload = rtp_packet.payload();
      std::string message;
      if (!payload.empty()) {
        message.assign(reinterpret_cast<const char*>(payload.data()),
                       payload.size());
      }
      printf("[%s] RTP recv ssrc=%u seq=%u ts=%u payload=\"%s\"\n",
             name_.c_str(), rtp_packet.Ssrc(), rtp_packet.SequenceNumber(),
             rtp_packet.Timestamp(), message.c_str());
    }));
  }

  const std::string name_;
  Clock* const clock_;
  TaskRunnerBase* const worker_queue_;
  SocketThread* const network_thread_;
  UdpTransport transport_;
  const uint32_t local_ssrc_;
  const uint32_t remote_ssrc_;
  const uint16_t rtp_port_;
  const uint16_t rtcp_port_;
  const SocketAddress remote_rtp_addr_;
  const SocketAddress remote_rtcp_addr_;

  AsyncUDPSocket* rtp_socket_ = nullptr;
  AsyncUDPSocket* rtcp_socket_ = nullptr;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  std::unique_ptr<ModuleRtpRtcpImpl2> module_;
  uint32_t next_timestamp_;
};

}  // namespace

int main() {
  Clock* clock = Clock::GetRealTimeClock();
  SocketThread network_thread;
  network_thread.Start();

  auto task_runner_factory = ave::base::CreateDefaultTaskRunnerFactory();
  auto worker_queue = task_runner_factory->CreateTaskRunner(
      "rtp_rtcp_demo_worker", ave::base::TaskRunnerFactory::Priority::NORMAL);

  DemoEndpoint endpoint_a(
      "A", clock, worker_queue.get(), &network_thread, kRtpPortA, kRtcpPortA,
      SocketAddress("127.0.0.1", kRtpPortB),
      SocketAddress("127.0.0.1", kRtcpPortB), kSsrcA, kSsrcB);
  DemoEndpoint endpoint_b(
      "B", clock, worker_queue.get(), &network_thread, kRtpPortB, kRtcpPortB,
      SocketAddress("127.0.0.1", kRtpPortA),
      SocketAddress("127.0.0.1", kRtcpPortA), kSsrcB, kSsrcA);

  endpoint_a.Initialize();
  endpoint_b.Initialize();

  for (int i = 0; i < 3; ++i) {
    endpoint_a.SendRtpMessage("hello-" + std::to_string(i));
    endpoint_b.SendRtpMessage("world-" + std::to_string(i));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  endpoint_a.SendRtcpReport();
  endpoint_b.SendRtcpReport();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  worker_queue->PostDelayedTaskAndWait(ave::base::toTask([]() {}), 0, true);

  endpoint_a.Stop();
  endpoint_b.Stop();
  network_thread.Stop();

  return 0;
}
