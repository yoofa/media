#ifndef MEDIA_MODULES_RTP_SESSION_TRANSPORT_PACKET_TRANSPORT_H_
#define MEDIA_MODULES_RTP_SESSION_TRANSPORT_PACKET_TRANSPORT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "base/errors.h"

namespace ave::media::rtp_session {

class PacketTransportSink;

class PacketEndpoint {
 public:
  static std::optional<PacketEndpoint> CreateIpv4(std::string_view address,
                                                  uint16_t port);
  static PacketEndpoint FromIpv4Bytes(std::array<uint8_t, 4> address,
                                      uint16_t port);

  PacketEndpoint() = default;

  bool IsValid() const;
  bool IsUnspecified() const;
  std::string AddressString() const;

  const std::array<uint8_t, 4>& ipv4_address() const { return ipv4_address_; }
  uint16_t port() const { return port_; }

  bool operator==(const PacketEndpoint& other) const = default;

 private:
  PacketEndpoint(std::array<uint8_t, 4> address, uint16_t port, bool valid);

  std::array<uint8_t, 4> ipv4_address_{};
  uint16_t port_ = 0;
  bool valid_ = false;
};

class PacketTransport {
 public:
  static constexpr size_t kMaximumUdpPayloadSize = 65507;

  virtual ~PacketTransport() = default;

  virtual status_t Start(PacketTransportSink* sink) = 0;
  virtual status_t Stop() = 0;
  virtual status_t SendPacket(std::span<const uint8_t> packet,
                              const PacketEndpoint& destination) = 0;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_TRANSPORT_PACKET_TRANSPORT_H_
