#ifndef MEDIA_MODULES_RTP_SESSION_SECURITY_SRTP_SESSION_H_
#define MEDIA_MODULES_RTP_SESSION_SECURITY_SRTP_SESSION_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "base/errors.h"

namespace ave::media::rtp_session {

enum class RtpSecurityMode : uint8_t {
  kNone,
  kSrtpPreSharedKey,
};

enum class SrtpPacketType : uint8_t {
  kRtp,
  kRtcp,
};

struct SrtpKey {
  static constexpr size_t kSize = 30;

  std::array<uint8_t, kSize> bytes{};

  friend bool operator==(const SrtpKey&, const SrtpKey&) = default;
};

struct SrtpSessionConfig {
  RtpSecurityMode mode = RtpSecurityMode::kNone;
  SrtpKey outbound_key;
  SrtpKey inbound_key;
  uint64_t replay_window_size = 128;
};

// Owns independent outbound and inbound libSRTP contexts. The pre-shared-key
// mode uses the WebRTC-compatible AES_CM_128_HMAC_SHA1_80 profile for both RTP
// and RTCP. Instances may be created and destroyed concurrently.
class SrtpSession final {
 public:
  static constexpr size_t kMaximumRtpOverhead = 144;
  static constexpr size_t kMaximumRtcpOverhead = 148;

  explicit SrtpSession(SrtpSessionConfig config);
  ~SrtpSession();

  SrtpSession(const SrtpSession&) = delete;
  SrtpSession& operator=(const SrtpSession&) = delete;

  status_t init_status() const;
  RtpSecurityMode mode() const;

  status_t Protect(SrtpPacketType packet_type,
                   std::span<const uint8_t> packet,
                   std::vector<uint8_t>* protected_packet);
  status_t Unprotect(SrtpPacketType packet_type,
                     std::span<const uint8_t> packet,
                     std::vector<uint8_t>* unprotected_packet);

  static constexpr size_t MaximumOverhead(SrtpPacketType packet_type) {
    return packet_type == SrtpPacketType::kRtp ? kMaximumRtpOverhead
                                               : kMaximumRtcpOverhead;
  }

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

bool IsRtcpMuxPacket(std::span<const uint8_t> packet);

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_SECURITY_SRTP_SESSION_H_
