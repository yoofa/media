#include "media/modules/rtp_session/transport/rtp_transport.h"

#include <algorithm>
#include <utility>

#include "base/time_utils.h"

namespace ave::media::rtp_session {

RtpTransport::RtpTransport(Config config)
    : packet_transport_(std::move(config.packet_transport)),
      srtp_session_(std::make_unique<SrtpSession>(std::move(config.security))),
      observer_(config.observer),
      remote_endpoint_(std::move(config.remote_endpoint)) {
  if (!remote_endpoint_.has_value() || !remote_endpoint_->IsValid() ||
      remote_endpoint_->IsUnspecified()) {
    remote_endpoint_.reset();
  } else {
    remote_rtcp_endpoints_.push_back(*remote_endpoint_);
  }
}

RtpTransport::~RtpTransport() {
  Stop();
}

status_t RtpTransport::Start(PacketReceivedCallback callback) {
  if (!callback || packet_transport_ == nullptr || srtp_session_ == nullptr) {
    return BAD_VALUE;
  }
  if (srtp_session_->init_status() != OK) {
    return srtp_session_->init_status();
  }

  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  {
    std::scoped_lock callback_lock(callback_mutex_);
    if (receive_callback_) {
      return ALREADY_EXISTS;
    }
    receive_callback_ = std::move(callback);
  }

  const status_t status = packet_transport_->Start(this);
  if (status != OK) {
    std::scoped_lock callback_lock(callback_mutex_);
    receive_callback_ = nullptr;
  }
  return status;
}

status_t RtpTransport::Stop() {
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  if (packet_transport_ == nullptr) {
    return NO_INIT;
  }

  const status_t status = packet_transport_->Stop();
  if (status == OK) {
    std::scoped_lock callback_lock(callback_mutex_);
    receive_callback_ = nullptr;
  }
  return status;
}

status_t RtpTransport::SetRemoteEndpoint(const PacketEndpoint& endpoint) {
  if (!endpoint.IsValid() || endpoint.IsUnspecified()) {
    return BAD_VALUE;
  }

  std::scoped_lock endpoints_lock(endpoints_mutex_);
  remote_endpoint_ = endpoint;
  remote_rtcp_endpoints_.clear();
  remote_rtcp_endpoints_.push_back(endpoint);
  return OK;
}

status_t RtpTransport::AddRemoteRtcpEndpoint(const PacketEndpoint& endpoint) {
  if (!endpoint.IsValid() || endpoint.IsUnspecified()) {
    return BAD_VALUE;
  }

  std::scoped_lock endpoints_lock(endpoints_mutex_);
  AddRemoteRtcpEndpointLocked(endpoint);
  return OK;
}

bool RtpTransport::SendRtp(std::span<const uint8_t> packet,
                           const rtp_rtcp::PacketOptions& options) {
  static_cast<void>(options);
  std::optional<PacketEndpoint> destination;
  {
    std::scoped_lock endpoints_lock(endpoints_mutex_);
    destination = remote_endpoint_;
  }
  if (!destination.has_value() || packet_transport_ == nullptr) {
    NotifyPacketSent(RtpTransportPacketType::kRtp, packet.size(), 0,
                     INVALID_OPERATION);
    return false;
  }

  std::unique_lock<std::mutex> security_lock(outbound_security_mutex_,
                                             std::defer_lock);
  std::span<const uint8_t> packet_to_send = packet;
  if (srtp_session_->mode() != RtpSecurityMode::kNone) {
    security_lock.lock();
    if (packet.size() >
        PacketTransport::kMaximumUdpPayloadSize -
            SrtpSession::MaximumOverhead(SrtpPacketType::kRtp)) {
      NotifyPacketSent(RtpTransportPacketType::kRtp, packet.size(), 0,
                       BAD_VALUE);
      return false;
    }
    const status_t protect_status = srtp_session_->Protect(
        SrtpPacketType::kRtp, packet, &outbound_security_buffer_);
    if (protect_status != OK) {
      NotifyPacketSent(RtpTransportPacketType::kRtp, packet.size(), 0,
                       protect_status);
      return false;
    }
    packet_to_send = outbound_security_buffer_;
  }

  const int64_t start_time_us = base::TimeMicros();
  const status_t status =
      packet_transport_->SendPacket(packet_to_send, *destination);
  NotifyPacketSent(RtpTransportPacketType::kRtp, packet_to_send.size(),
                   base::TimeMicros() - start_time_us, status);
  return status == OK;
}

bool RtpTransport::SendRtcp(std::span<const uint8_t> packet) {
  std::vector<PacketEndpoint> destinations;
  {
    std::scoped_lock endpoints_lock(endpoints_mutex_);
    destinations = remote_rtcp_endpoints_;
  }
  if (destinations.empty() || packet_transport_ == nullptr) {
    NotifyPacketSent(RtpTransportPacketType::kRtcp, packet.size(), 0,
                     INVALID_OPERATION);
    return false;
  }

  std::unique_lock<std::mutex> security_lock(outbound_security_mutex_,
                                             std::defer_lock);
  std::span<const uint8_t> packet_to_send = packet;
  if (srtp_session_->mode() != RtpSecurityMode::kNone) {
    security_lock.lock();
    if (packet.size() >
        PacketTransport::kMaximumUdpPayloadSize -
            SrtpSession::MaximumOverhead(SrtpPacketType::kRtcp)) {
      NotifyPacketSent(RtpTransportPacketType::kRtcp, packet.size(), 0,
                       BAD_VALUE);
      return false;
    }
    const status_t protect_status = srtp_session_->Protect(
        SrtpPacketType::kRtcp, packet, &outbound_security_buffer_);
    if (protect_status != OK) {
      NotifyPacketSent(RtpTransportPacketType::kRtcp, packet.size(), 0,
                       protect_status);
      return false;
    }
    packet_to_send = outbound_security_buffer_;
  }

  bool success = true;
  for (const PacketEndpoint& destination : destinations) {
    const int64_t start_time_us = base::TimeMicros();
    const status_t status =
        packet_transport_->SendPacket(packet_to_send, destination);
    NotifyPacketSent(RtpTransportPacketType::kRtcp, packet_to_send.size(),
                     base::TimeMicros() - start_time_us, status);
    success = success && status == OK;
  }
  return success;
}

void RtpTransport::OnPacketReceived(std::span<const uint8_t> packet,
                                    const PacketEndpoint& source,
                                    base::Timestamp arrival_time) {
  const RtpTransportPacketType transport_packet_type =
      IsRtcpMuxPacket(packet) ? RtpTransportPacketType::kRtcp
                              : RtpTransportPacketType::kRtp;
  std::unique_lock<std::mutex> security_lock(inbound_security_mutex_,
                                             std::defer_lock);
  if (srtp_session_->mode() != RtpSecurityMode::kNone) {
    security_lock.lock();
    const SrtpPacketType srtp_packet_type =
        transport_packet_type == RtpTransportPacketType::kRtcp
            ? SrtpPacketType::kRtcp
            : SrtpPacketType::kRtp;
    const status_t status = srtp_session_->Unprotect(srtp_packet_type, packet,
                                                     &inbound_security_buffer_);
    if (status != OK) {
      NotifyPacketDiscarded(transport_packet_type, status);
      return;
    }
    packet = inbound_security_buffer_;
  }

  std::scoped_lock callback_lock(callback_mutex_);
  if (receive_callback_) {
    receive_callback_(packet, source, arrival_time);
  }
}

void RtpTransport::NotifyPacketDiscarded(RtpTransportPacketType packet_type,
                                         status_t status) const {
  if (observer_ != nullptr) {
    observer_->OnPacketDiscarded(packet_type, status);
  }
}

void RtpTransport::NotifyPacketSent(RtpTransportPacketType packet_type,
                                    size_t bytes,
                                    int64_t send_duration_us,
                                    status_t status) const {
  if (observer_ != nullptr) {
    observer_->OnPacketSent(packet_type, bytes, send_duration_us, status);
  }
}

void RtpTransport::AddRemoteRtcpEndpointLocked(const PacketEndpoint& endpoint) {
  if (std::ranges::find(remote_rtcp_endpoints_, endpoint) ==
      remote_rtcp_endpoints_.end()) {
    remote_rtcp_endpoints_.push_back(endpoint);
  }
}

}  // namespace ave::media::rtp_session
