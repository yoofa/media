#ifndef MEDIA_MODULES_RTP_SESSION_TRANSPORT_UDP_PACKET_TRANSPORT_H_
#define MEDIA_MODULES_RTP_SESSION_TRANSPORT_UDP_PACKET_TRANSPORT_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

#include "base/clock.h"
#include "media/modules/rtp_session/transport/packet_transport.h"

namespace ave::media::rtp_session {

class UdpPacketTransport final : public PacketTransport {
 public:
  struct Config {
    PacketEndpoint local_endpoint;
    base::Clock* clock = nullptr;
    size_t maximum_packet_size = PacketTransport::kMaximumUdpPayloadSize;
  };

  explicit UdpPacketTransport(Config config);
  ~UdpPacketTransport() override;

  status_t Start(PacketTransportSink* sink) override;
  status_t Stop() override;
  status_t SendPacket(std::span<const uint8_t> packet,
                      const PacketEndpoint& destination) override;

 private:
  void ReceiveLoop();
  void CloseDescriptorsLocked();

  const Config config_;

  std::mutex lifecycle_mutex_;
  std::mutex state_mutex_;
  int socket_fd_ = -1;
  int wakeup_read_fd_ = -1;
  int wakeup_write_fd_ = -1;
  std::atomic<bool> running_{false};
  PacketTransportSink* sink_ = nullptr;
  std::thread receive_thread_;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_TRANSPORT_UDP_PACKET_TRANSPORT_H_
