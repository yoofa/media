#include "media/modules/rtp_session/transport/packet_transport.h"

#include <arpa/inet.h>

#include <algorithm>

namespace ave::media::rtp_session {

PacketEndpoint::PacketEndpoint(std::array<uint8_t, 4> address,
                               uint16_t port,
                               bool valid)
    : ipv4_address_(address), port_(port), valid_(valid) {}

std::optional<PacketEndpoint> PacketEndpoint::CreateIpv4(
    std::string_view address,
    uint16_t port) {
  if (address.empty() || port == 0) {
    return std::nullopt;
  }

  in_addr parsed_address{};
  const std::string null_terminated_address(address);
  if (inet_pton(AF_INET, null_terminated_address.c_str(), &parsed_address) !=
      1) {
    return std::nullopt;
  }

  std::array<uint8_t, 4> address_bytes{};
  const auto* source = reinterpret_cast<const uint8_t*>(&parsed_address.s_addr);
  std::copy_n(source, address_bytes.size(), address_bytes.begin());
  return PacketEndpoint(address_bytes, port, true);
}

PacketEndpoint PacketEndpoint::FromIpv4Bytes(std::array<uint8_t, 4> address,
                                             uint16_t port) {
  return {address, port, port != 0};
}

bool PacketEndpoint::IsValid() const {
  return valid_ && port_ != 0;
}

bool PacketEndpoint::IsUnspecified() const {
  return std::ranges::all_of(ipv4_address_,
                             [](uint8_t value) { return value == 0; });
}

std::string PacketEndpoint::AddressString() const {
  if (!valid_) {
    return {};
  }

  in_addr address{};
  std::ranges::copy(ipv4_address_, reinterpret_cast<uint8_t*>(&address.s_addr));
  std::array<char, INET_ADDRSTRLEN> text{};
  if (inet_ntop(AF_INET, &address, text.data(), text.size()) == nullptr) {
    return {};
  }
  return text.data();
}

}  // namespace ave::media::rtp_session
