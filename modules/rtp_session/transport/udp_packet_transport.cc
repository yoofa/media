#include "media/modules/rtp_session/transport/udp_packet_transport.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <utility>
#include <vector>

#include "media/modules/rtp_session/transport/packet_transport_sink.h"

namespace ave::media::rtp_session {
namespace {

sockaddr_in ToSockaddr(const PacketEndpoint& endpoint) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(endpoint.port());
  std::memcpy(&address.sin_addr.s_addr, endpoint.ipv4_address().data(),
              endpoint.ipv4_address().size());
  return address;
}

PacketEndpoint FromSockaddr(const sockaddr_in& address) {
  std::array<uint8_t, 4> bytes{};
  std::memcpy(bytes.data(), &address.sin_addr.s_addr, bytes.size());
  return PacketEndpoint::FromIpv4Bytes(bytes, ntohs(address.sin_port));
}

status_t StatusFromErrno() {
  return errno == 0 ? UNKNOWN_ERROR : -errno;
}

}  // namespace

UdpPacketTransport::UdpPacketTransport(Config config)
    : config_(std::move(config)) {}

UdpPacketTransport::~UdpPacketTransport() {
  Stop();
}

status_t UdpPacketTransport::Start(PacketTransportSink* sink) {
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  if (sink == nullptr || !config_.local_endpoint.IsValid() ||
      config_.clock == nullptr || config_.maximum_packet_size == 0 ||
      config_.maximum_packet_size > PacketTransport::kMaximumUdpPayloadSize) {
    return BAD_VALUE;
  }
  if (running_.load(std::memory_order_acquire)) {
    return ALREADY_EXISTS;
  }

  const int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    return StatusFromErrno();
  }

  const int reuse_address = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                 sizeof(reuse_address)) != 0) {
    const status_t status = StatusFromErrno();
    close(socket_fd);
    return status;
  }

  const sockaddr_in local_address = ToSockaddr(config_.local_endpoint);
  if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&local_address),
           sizeof(local_address)) != 0) {
    const status_t status = StatusFromErrno();
    close(socket_fd);
    return status;
  }

  std::array<int, 2> wakeup_pipe{};
  if (pipe(wakeup_pipe.data()) != 0) {
    const status_t status = StatusFromErrno();
    close(socket_fd);
    return status;
  }

  {
    std::scoped_lock state_lock(state_mutex_);
    socket_fd_ = socket_fd;
    wakeup_read_fd_ = wakeup_pipe[0];
    wakeup_write_fd_ = wakeup_pipe[1];
    sink_ = sink;
    running_.store(true, std::memory_order_release);
  }

  try {
    receive_thread_ = std::thread(&UdpPacketTransport::ReceiveLoop, this);
  } catch (const std::system_error&) {
    std::scoped_lock state_lock(state_mutex_);
    running_.store(false, std::memory_order_release);
    sink_ = nullptr;
    CloseDescriptorsLocked();
    return NO_MEMORY;
  }
  return OK;
}

status_t UdpPacketTransport::Stop() {
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  if (!running_.load(std::memory_order_acquire)) {
    return OK;
  }
  if (receive_thread_.joinable() &&
      receive_thread_.get_id() == std::this_thread::get_id()) {
    return INVALID_OPERATION;
  }

  int wakeup_fd = -1;
  {
    std::scoped_lock state_lock(state_mutex_);
    running_.store(false, std::memory_order_release);
    wakeup_fd = wakeup_write_fd_;
  }
  if (wakeup_fd >= 0) {
    constexpr uint8_t kWakeupByte = 1;
    while (write(wakeup_fd, &kWakeupByte, sizeof(kWakeupByte)) < 0 &&
           errno == EINTR) {
    }
  }
  if (receive_thread_.joinable()) {
    receive_thread_.join();
  }

  std::scoped_lock state_lock(state_mutex_);
  sink_ = nullptr;
  CloseDescriptorsLocked();
  return OK;
}

status_t UdpPacketTransport::SendPacket(std::span<const uint8_t> packet,
                                        const PacketEndpoint& destination) {
  if (packet.empty() ||
      packet.size() > PacketTransport::kMaximumUdpPayloadSize ||
      !destination.IsValid() || destination.IsUnspecified()) {
    return BAD_VALUE;
  }

  std::scoped_lock state_lock(state_mutex_);
  if (!running_.load(std::memory_order_acquire) || socket_fd_ < 0) {
    return INVALID_OPERATION;
  }

  const sockaddr_in destination_address = ToSockaddr(destination);
  const ssize_t sent =
      sendto(socket_fd_, packet.data(), packet.size(), 0,
             reinterpret_cast<const sockaddr*>(&destination_address),
             sizeof(destination_address));
  if (sent < 0) {
    return StatusFromErrno();
  }
  return std::cmp_equal(sent, packet.size()) ? OK : UNKNOWN_ERROR;
}

void UdpPacketTransport::ReceiveLoop() {
  int socket_fd = -1;
  int wakeup_fd = -1;
  {
    std::scoped_lock state_lock(state_mutex_);
    socket_fd = socket_fd_;
    wakeup_fd = wakeup_read_fd_;
  }

  std::vector<uint8_t> buffer(config_.maximum_packet_size);
  std::array<pollfd, 2> poll_descriptors{{
      {.fd = socket_fd, .events = POLLIN, .revents = 0},
      {.fd = wakeup_fd, .events = POLLIN, .revents = 0},
  }};
  while (running_.load(std::memory_order_acquire)) {
    const int poll_result =
        poll(poll_descriptors.data(), poll_descriptors.size(), -1);
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if ((poll_descriptors[1].revents & POLLIN) != 0) {
      break;
    }
    if ((poll_descriptors[0].revents & POLLIN) == 0) {
      continue;
    }

    sockaddr_in source_address{};
    iovec io_vector{
        .iov_base = buffer.data(),
        .iov_len = buffer.size(),
    };
    msghdr message{
        .msg_name = &source_address,
        .msg_namelen = sizeof(source_address),
        .msg_iov = &io_vector,
        .msg_iovlen = 1,
    };
    const ssize_t received = recvmsg(socket_fd, &message, MSG_TRUNC);
    const base::Timestamp arrival_time = config_.clock->CurrentTime();
    if (received <= 0 || source_address.sin_family != AF_INET ||
        !std::cmp_less_equal(received, buffer.size()) ||
        (message.msg_flags & MSG_TRUNC) != 0) {
      continue;
    }

    PacketTransportSink* sink = nullptr;
    {
      std::scoped_lock state_lock(state_mutex_);
      if (running_.load(std::memory_order_acquire)) {
        sink = sink_;
      }
    }
    if (sink != nullptr) {
      sink->OnPacketReceived(std::span<const uint8_t>(
                                 buffer.data(), static_cast<size_t>(received)),
                             FromSockaddr(source_address), arrival_time);
    }
  }
}

void UdpPacketTransport::CloseDescriptorsLocked() {
  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
  }
  if (wakeup_read_fd_ >= 0) {
    close(wakeup_read_fd_);
    wakeup_read_fd_ = -1;
  }
  if (wakeup_write_fd_ >= 0) {
    close(wakeup_write_fd_);
    wakeup_write_fd_ = -1;
  }
}

}  // namespace ave::media::rtp_session
