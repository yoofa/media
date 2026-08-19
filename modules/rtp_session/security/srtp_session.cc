#include "media/modules/rtp_session/security/srtp_session.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

#include "third_party/libsrtp/include/srtp.h"

namespace ave::media::rtp_session {
namespace {

static_assert(SrtpSession::kMaximumRtpOverhead >= SRTP_MAX_TRAILER_LEN);
static_assert(SrtpSession::kMaximumRtcpOverhead >= SRTP_MAX_SRTCP_TRAILER_LEN);

struct LibSrtpGlobalState {
  std::mutex mutex;
  uint64_t reference_count = 0;
};

LibSrtpGlobalState& GetLibSrtpGlobalState() {
  static LibSrtpGlobalState state;
  return state;
}

status_t StatusFromSrtpError(srtp_err_status_t error) {
  switch (error) {
    case srtp_err_status_ok:
      return OK;
    case srtp_err_status_bad_param:
    case srtp_err_status_parse_err:
      return BAD_VALUE;
    case srtp_err_status_alloc_fail:
      return NO_MEMORY;
    case srtp_err_status_auth_fail:
    case srtp_err_status_cipher_fail:
    case srtp_err_status_bad_mki:
      return PERMISSION_DENIED;
    case srtp_err_status_replay_fail:
    case srtp_err_status_replay_old:
    case srtp_err_status_pkt_idx_old:
      return ALREADY_EXISTS;
    case srtp_err_status_no_ctx:
    case srtp_err_status_init_fail:
      return NO_INIT;
    default:
      return UNKNOWN_ERROR;
  }
}

status_t AcquireLibSrtp() {
  LibSrtpGlobalState& state = GetLibSrtpGlobalState();
  std::scoped_lock lock(state.mutex);
  if (state.reference_count == 0) {
    const status_t status = StatusFromSrtpError(srtp_init());
    if (status != OK) {
      return status;
    }
  }
  ++state.reference_count;
  return OK;
}

void ReleaseLibSrtp() {
  LibSrtpGlobalState& state = GetLibSrtpGlobalState();
  std::scoped_lock lock(state.mutex);
  if (state.reference_count == 0) {
    return;
  }
  --state.reference_count;
  if (state.reference_count == 0) {
    static_cast<void>(srtp_shutdown());
  }
}

srtp_policy_t CreatePolicy(SrtpKey* key,
                           srtp_ssrc_type_t ssrc_type,
                           uint64_t replay_window_size) {
  srtp_policy_t policy{};
  srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
  policy.ssrc.type = ssrc_type;
  policy.key = key->bytes.data();
  policy.window_size = static_cast<unsigned long>(replay_window_size);
  policy.allow_repeat_tx = 1;
  return policy;
}

}  // namespace

class SrtpSession::Impl final {
 public:
  explicit Impl(SrtpSessionConfig config) : config_(std::move(config)) {
    if (config_.mode == RtpSecurityMode::kNone) {
      init_status_ = OK;
      return;
    }
    if (config_.mode != RtpSecurityMode::kSrtpPreSharedKey ||
        config_.replay_window_size == 0 ||
        config_.replay_window_size >
            static_cast<uint64_t>(std::numeric_limits<unsigned long>::max())) {
      init_status_ = BAD_VALUE;
      return;
    }

    init_status_ = AcquireLibSrtp();
    if (init_status_ != OK) {
      return;
    }
    owns_global_reference_ = true;

    srtp_policy_t outbound_policy = CreatePolicy(
        &config_.outbound_key, ssrc_any_outbound, config_.replay_window_size);
    init_status_ =
        StatusFromSrtpError(srtp_create(&outbound_context_, &outbound_policy));
    if (init_status_ != OK) {
      return;
    }

    srtp_policy_t inbound_policy = CreatePolicy(
        &config_.inbound_key, ssrc_any_inbound, config_.replay_window_size);
    init_status_ =
        StatusFromSrtpError(srtp_create(&inbound_context_, &inbound_policy));
  }

  ~Impl() {
    if (outbound_context_ != nullptr) {
      static_cast<void>(srtp_dealloc(outbound_context_));
    }
    if (inbound_context_ != nullptr) {
      static_cast<void>(srtp_dealloc(inbound_context_));
    }
    if (owns_global_reference_) {
      ReleaseLibSrtp();
    }
  }

  status_t init_status() const { return init_status_; }
  RtpSecurityMode mode() const { return config_.mode; }

  status_t Protect(SrtpPacketType packet_type,
                   std::span<const uint8_t> packet,
                   std::vector<uint8_t>* protected_packet) {
    if (protected_packet == nullptr || packet.empty()) {
      return BAD_VALUE;
    }
    if (init_status_ != OK) {
      return init_status_;
    }
    if (config_.mode == RtpSecurityMode::kNone) {
      protected_packet->assign(packet.begin(), packet.end());
      return OK;
    }
    if (packet.size() > static_cast<size_t>(std::numeric_limits<int>::max()) -
                            SrtpSession::MaximumOverhead(packet_type)) {
      return BAD_VALUE;
    }

    protected_packet->assign(
        packet.size() + SrtpSession::MaximumOverhead(packet_type), 0);
    std::ranges::copy(packet, protected_packet->begin());
    int packet_size = static_cast<int>(packet.size());
    std::scoped_lock context_lock(outbound_mutex_);
    const srtp_err_status_t error =
        packet_type == SrtpPacketType::kRtp
            ? srtp_protect(outbound_context_, protected_packet->data(),
                           &packet_size)
            : srtp_protect_rtcp(outbound_context_, protected_packet->data(),
                                &packet_size);
    const status_t status = StatusFromSrtpError(error);
    if (status != OK) {
      protected_packet->clear();
      return status;
    }
    protected_packet->resize(static_cast<size_t>(packet_size));
    return OK;
  }

  status_t Unprotect(SrtpPacketType packet_type,
                     std::span<const uint8_t> packet,
                     std::vector<uint8_t>* unprotected_packet) {
    if (unprotected_packet == nullptr || packet.empty() ||
        packet.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
      return BAD_VALUE;
    }
    if (init_status_ != OK) {
      return init_status_;
    }
    if (config_.mode == RtpSecurityMode::kNone) {
      unprotected_packet->assign(packet.begin(), packet.end());
      return OK;
    }

    unprotected_packet->assign(packet.begin(), packet.end());
    int packet_size = static_cast<int>(packet.size());
    std::scoped_lock context_lock(inbound_mutex_);
    const srtp_err_status_t error =
        packet_type == SrtpPacketType::kRtp
            ? srtp_unprotect(inbound_context_, unprotected_packet->data(),
                             &packet_size)
            : srtp_unprotect_rtcp(inbound_context_, unprotected_packet->data(),
                                  &packet_size);
    const status_t status = StatusFromSrtpError(error);
    if (status != OK) {
      unprotected_packet->clear();
      return status;
    }
    unprotected_packet->resize(static_cast<size_t>(packet_size));
    return OK;
  }

 private:
  SrtpSessionConfig config_;
  status_t init_status_ = NO_INIT;
  bool owns_global_reference_ = false;
  srtp_t outbound_context_ = nullptr;
  srtp_t inbound_context_ = nullptr;
  std::mutex outbound_mutex_;
  std::mutex inbound_mutex_;
};

SrtpSession::SrtpSession(SrtpSessionConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

SrtpSession::~SrtpSession() = default;

status_t SrtpSession::init_status() const {
  return impl_->init_status();
}

RtpSecurityMode SrtpSession::mode() const {
  return impl_->mode();
}

status_t SrtpSession::Protect(SrtpPacketType packet_type,
                              std::span<const uint8_t> packet,
                              std::vector<uint8_t>* protected_packet) {
  return impl_->Protect(packet_type, packet, protected_packet);
}

status_t SrtpSession::Unprotect(SrtpPacketType packet_type,
                                std::span<const uint8_t> packet,
                                std::vector<uint8_t>* unprotected_packet) {
  return impl_->Unprotect(packet_type, packet, unprotected_packet);
}

bool IsRtcpMuxPacket(std::span<const uint8_t> packet) {
  if (packet.size() < 2) {
    return false;
  }
  const uint8_t packet_type = packet[1];
  return packet_type >= 192 && packet_type <= 223;
}

}  // namespace ave::media::rtp_session
