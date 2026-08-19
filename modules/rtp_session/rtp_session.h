#ifndef MEDIA_MODULES_RTP_SESSION_RTP_SESSION_H_
#define MEDIA_MODULES_RTP_SESSION_RTP_SESSION_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/errors.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sink_interface.h"
#include "media/modules/rtp_session/transport/packet_transport.h"

namespace ave::base {
class Clock;
}

namespace ave::media::rtp_rtcp {
class ReceiveStatistics;
class RtpPacketReceived;
class RtpRtcpInterface;
class Transport;
}  // namespace ave::media::rtp_rtcp

namespace ave::media::rtp_session {

class RtpTransport;

using RtpReceiveStreamId = uint64_t;

enum class RtpMediaType : uint8_t {
  kAudio,
  kVideo,
};

enum class RtpSessionDiscardReason : uint8_t {
  kInvalidRtpPacket,
  kInvalidRtcpPacket,
  kUnmatchedRtpPacket,
};

struct RtpReceiveStreamConfig {
  RtpMediaType media_type = RtpMediaType::kAudio;
  std::vector<uint8_t> payload_types;
  uint32_t clock_rate_hz = 0;
  std::optional<uint32_t> remote_ssrc;
  std::optional<std::string> mid;
  bool allow_payload_type_discovery = true;
};

struct RtpReceiveStreamStats {
  RtpReceiveStreamId stream_id = 0;
  RtpMediaType media_type = RtpMediaType::kAudio;
  uint64_t packets_received = 0;
  uint64_t bytes_received = 0;
  uint64_t payload_bytes_received = 0;
  std::vector<uint32_t> remote_ssrcs;
};

struct DiscoveredRtpReceiveStream {
  RtpReceiveStreamConfig config;
  std::shared_ptr<rtp_rtcp::RtpPacketSinkInterface> sink;
};

struct RtpSessionStats {
  uint64_t rtp_packets_received = 0;
  uint64_t rtcp_packets_received = 0;
  uint64_t invalid_packets = 0;
  uint64_t unmatched_rtp_packets = 0;
  std::vector<RtpReceiveStreamStats> receive_streams;
};

class RtpSessionObserver {
 public:
  virtual ~RtpSessionObserver() = default;

  virtual void OnRtpPacketReceived(
      const rtp_rtcp::RtpPacketReceived& /*packet*/,
      const PacketEndpoint& /*source*/) {}
  virtual void OnRtcpPacketReceived(std::span<const uint8_t> /*packet*/,
                                    const PacketEndpoint& /*source*/) {}
  virtual void OnSessionPacketDiscarded(RtpSessionDiscardReason reason,
                                        std::span<const uint8_t> packet,
                                        const PacketEndpoint& source) {}
};

class RtpSession final {
 public:
  struct Config {
    using ReceiveStreamFactory =
        std::function<std::optional<DiscoveredRtpReceiveStream>(
            const rtp_rtcp::RtpPacketReceived& packet,
            const PacketEndpoint& source)>;

    std::unique_ptr<RtpTransport> transport;
    base::Clock* clock = nullptr;
    std::optional<uint8_t> mid_extension_id;
    ReceiveStreamFactory receive_stream_factory;
    RtpSessionObserver* observer = nullptr;
  };

  explicit RtpSession(Config config);
  ~RtpSession();

  RtpSession(const RtpSession&) = delete;
  RtpSession& operator=(const RtpSession&) = delete;

  status_t init_status() const { return init_status_; }
  status_t Start();
  status_t Stop();
  bool started() const;

  status_t AddReceiveStream(
      RtpReceiveStreamConfig config,
      std::shared_ptr<rtp_rtcp::RtpPacketSinkInterface> sink,
      RtpReceiveStreamId* stream_id);
  status_t RemoveReceiveStream(RtpReceiveStreamId stream_id);

  // The session owns added modules. Modules may only be added or removed while
  // the session is stopped, so incoming RTCP callbacks cannot race destruction.
  status_t AddRtpRtcpModule(std::unique_ptr<rtp_rtcp::RtpRtcpInterface> module);
  status_t RemoveRtpRtcpModule(uint32_t local_ssrc);
  rtp_rtcp::RtpRtcpInterface* GetRtpRtcpModule(uint32_t local_ssrc) const;

  // Used when constructing RtpRtcpInterface modules that will subsequently be
  // transferred to this session.
  rtp_rtcp::Transport* outgoing_transport() const;
  rtp_rtcp::ReceiveStatistics* receive_statistics() const;

  RtpSessionStats GetStats() const;

 private:
  struct ReceiveStreamEntry {
    RtpReceiveStreamConfig config;
    std::shared_ptr<rtp_rtcp::RtpPacketSinkInterface> sink;
    RtpReceiveStreamStats stats;
  };

  void OnPacketReceived(std::span<const uint8_t> packet,
                        const PacketEndpoint& source,
                        base::Timestamp arrival_time);
  void HandleRtpPacket(std::span<const uint8_t> packet,
                       const PacketEndpoint& source,
                       base::Timestamp arrival_time);
  void HandleRtcpPacket(std::span<const uint8_t> packet,
                        const PacketEndpoint& source);
  void NotifyPacketDiscarded(RtpSessionDiscardReason reason,
                             std::span<const uint8_t> packet,
                             const PacketEndpoint& source) const;
  static bool ValidateRtcpPacket(std::span<const uint8_t> packet);

  status_t init_status_;
  RtpSessionObserver* const observer_;
  const Config::ReceiveStreamFactory receive_stream_factory_;
  std::unique_ptr<RtpTransport> transport_;
  std::unique_ptr<rtp_rtcp::ReceiveStatistics> receive_statistics_;
  rtp_rtcp::RtpHeaderExtensionMap extension_map_;

  mutable std::mutex lifecycle_mutex_;
  bool started_ = false;

  mutable std::mutex mutex_;
  RtpReceiveStreamId next_stream_id_ = 1;
  std::unordered_map<RtpReceiveStreamId, ReceiveStreamEntry> receive_streams_;
  std::unordered_map<uint32_t, RtpReceiveStreamId> ssrc_routes_;
  std::unordered_map<std::string, RtpReceiveStreamId> mid_routes_;
  std::unordered_map<uint32_t, std::unique_ptr<rtp_rtcp::RtpRtcpInterface>>
      rtp_rtcp_modules_;
  RtpSessionStats stats_;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_RTP_SESSION_H_
