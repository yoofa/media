#ifndef MEDIA_MODULES_RTP_SESSION_TRANSPORT_RTP_TRANSPORT_H_
#define MEDIA_MODULES_RTP_SESSION_TRANSPORT_RTP_TRANSPORT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/src/transport/transport.h"
#include "media/modules/rtp_session/transport/packet_transport.h"
#include "media/modules/rtp_session/transport/packet_transport_sink.h"

namespace ave::media::rtp_session {

enum class RtpTransportPacketType : uint8_t {
  kRtp,
  kRtcp,
};

class RtpTransportObserver {
 public:
  virtual ~RtpTransportObserver() = default;

  virtual void OnPacketSent(RtpTransportPacketType packet_type,
                            size_t bytes,
                            int64_t send_duration_us,
                            status_t status) = 0;
};

class RtpTransport final : public rtp_rtcp::Transport,
                           public PacketTransportSink {
 public:
  using PacketReceivedCallback =
      std::function<void(std::span<const uint8_t> packet,
                         const PacketEndpoint& source,
                         base::Timestamp arrival_time)>;

  struct Config {
    std::unique_ptr<PacketTransport> packet_transport;
    std::optional<PacketEndpoint> remote_endpoint;
    RtpTransportObserver* observer = nullptr;
  };

  explicit RtpTransport(Config config);
  ~RtpTransport() override;

  status_t Start(PacketReceivedCallback callback);
  status_t Stop();

  status_t SetRemoteEndpoint(const PacketEndpoint& endpoint);
  status_t AddRemoteRtcpEndpoint(const PacketEndpoint& endpoint);

  bool SendRtp(std::span<const uint8_t> packet,
               const rtp_rtcp::PacketOptions& options) override;
  bool SendRtcp(std::span<const uint8_t> packet) override;

  void OnPacketReceived(std::span<const uint8_t> packet,
                        const PacketEndpoint& source,
                        base::Timestamp arrival_time) override;

 private:
  void NotifyPacketSent(RtpTransportPacketType packet_type,
                        size_t bytes,
                        int64_t send_duration_us,
                        status_t status) const;
  void AddRemoteRtcpEndpointLocked(const PacketEndpoint& endpoint);

  std::unique_ptr<PacketTransport> packet_transport_;
  RtpTransportObserver* const observer_;

  std::mutex lifecycle_mutex_;
  std::mutex callback_mutex_;
  PacketReceivedCallback receive_callback_;

  std::mutex endpoints_mutex_;
  std::optional<PacketEndpoint> remote_endpoint_;
  std::vector<PacketEndpoint> remote_rtcp_endpoints_;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_TRANSPORT_RTP_TRANSPORT_H_
