#include "media/modules/rtp_session/rtp_session.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/clock.h"
#include "media/modules/rtp_rtcp/api/receive_statistics.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/common_header.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extensions.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"
#include "media/modules/rtp_session/security/srtp_session.h"
#include "media/modules/rtp_session/transport/rtp_transport.h"

namespace ave::media::rtp_session {
namespace {

bool ContainsPayloadType(const RtpReceiveStreamConfig& config,
                         uint8_t payload_type) {
  return std::ranges::find(config.payload_types, payload_type) !=
         config.payload_types.end();
}

}  // namespace

RtpSession::RtpSession(Config config)
    : init_status_(config.transport == nullptr ? BAD_VALUE : OK),
      observer_(config.observer),
      receive_stream_factory_(std::move(config.receive_stream_factory)),
      transport_(std::move(config.transport)),
      receive_statistics_(rtp_rtcp::ReceiveStatistics::Create(
          config.clock != nullptr ? config.clock
                                  : base::Clock::GetRealTimeClock())) {
  if (config.mid_extension_id.has_value()) {
    if (*config.mid_extension_id == 0 ||
        !extension_map_.Register<rtp_rtcp::RtpMid>(*config.mid_extension_id)) {
      init_status_ = BAD_VALUE;
    }
  }
}

RtpSession::~RtpSession() {
  Stop();
}

status_t RtpSession::Start() {
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  if (init_status_ != OK) {
    return init_status_;
  }
  if (started_) {
    return ALREADY_EXISTS;
  }

  const status_t status = transport_->Start(
      [this](std::span<const uint8_t> packet, const PacketEndpoint& source,
             base::Timestamp arrival_time) {
        OnPacketReceived(packet, source, arrival_time);
      });
  if (status == OK) {
    started_ = true;
  }
  return status;
}

status_t RtpSession::Stop() {
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  if (!started_) {
    return OK;
  }
  const status_t status = transport_->Stop();
  if (status == OK) {
    started_ = false;
  }
  return status;
}

bool RtpSession::started() const {
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  return started_;
}

status_t RtpSession::AddReceiveStream(
    RtpReceiveStreamConfig config,
    std::shared_ptr<rtp_rtcp::RtpPacketSinkInterface> sink,
    RtpReceiveStreamId* stream_id) {
  if (sink == nullptr || stream_id == nullptr || config.clock_rate_hz == 0 ||
      config.payload_types.empty()) {
    return BAD_VALUE;
  }
  if (config.clock_rate_hz >
          static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
      std::ranges::any_of(
          config.payload_types,
          [](uint8_t payload_type) { return payload_type > 127; }) ||
      (config.remote_ssrc.has_value() && *config.remote_ssrc == 0)) {
    return BAD_VALUE;
  }
  if (config.mid.has_value() &&
      (config.mid->empty() ||
       config.mid->size() > rtp_rtcp::RtpMid::kMaxValueSizeBytes)) {
    return BAD_VALUE;
  }
  if (config.mid.has_value() &&
      !extension_map_.IsRegistered(rtp_rtcp::RtpMid::kId)) {
    return INVALID_OPERATION;
  }

  std::ranges::sort(config.payload_types);
  if (std::ranges::adjacent_find(config.payload_types) !=
      config.payload_types.end()) {
    return BAD_VALUE;
  }

  std::scoped_lock lock(mutex_);
  if (config.remote_ssrc.has_value() &&
      ssrc_routes_.contains(*config.remote_ssrc)) {
    return ALREADY_EXISTS;
  }
  if (config.mid.has_value() && mid_routes_.contains(*config.mid)) {
    return ALREADY_EXISTS;
  }

  const RtpReceiveStreamId new_stream_id = next_stream_id_++;
  RtpReceiveStreamStats stream_stats{
      .stream_id = new_stream_id,
      .media_type = config.media_type,
  };
  if (config.remote_ssrc.has_value()) {
    stream_stats.remote_ssrcs.push_back(*config.remote_ssrc);
  }
  receive_streams_.emplace(new_stream_id, ReceiveStreamEntry{
                                              .config = std::move(config),
                                              .sink = std::move(sink),
                                              .stats = std::move(stream_stats),
                                          });
  ReceiveStreamEntry& entry = receive_streams_.at(new_stream_id);
  if (entry.config.remote_ssrc.has_value()) {
    ssrc_routes_.emplace(*entry.config.remote_ssrc, new_stream_id);
  }
  if (entry.config.mid.has_value()) {
    mid_routes_.emplace(*entry.config.mid, new_stream_id);
  }
  *stream_id = new_stream_id;
  return OK;
}

status_t RtpSession::RemoveReceiveStream(RtpReceiveStreamId stream_id) {
  std::scoped_lock lock(mutex_);
  const auto stream_it = receive_streams_.find(stream_id);
  if (stream_it == receive_streams_.end()) {
    return NAME_NOT_FOUND;
  }

  std::erase_if(ssrc_routes_, [stream_id](const auto& route) {
    return route.second == stream_id;
  });
  std::erase_if(mid_routes_, [stream_id](const auto& route) {
    return route.second == stream_id;
  });
  receive_streams_.erase(stream_it);
  return OK;
}

status_t RtpSession::AddRtpRtcpModule(
    std::unique_ptr<rtp_rtcp::RtpRtcpInterface> module) {
  if (module == nullptr) {
    return BAD_VALUE;
  }
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  if (started_) {
    return INVALID_OPERATION;
  }
  std::scoped_lock lock(mutex_);
  const uint32_t local_ssrc = module->SSRC();
  if (rtp_rtcp_modules_.contains(local_ssrc)) {
    return ALREADY_EXISTS;
  }
  rtp_rtcp_modules_.emplace(local_ssrc, std::move(module));
  return OK;
}

status_t RtpSession::RemoveRtpRtcpModule(uint32_t local_ssrc) {
  std::scoped_lock lifecycle_lock(lifecycle_mutex_);
  if (started_) {
    return INVALID_OPERATION;
  }
  std::scoped_lock lock(mutex_);
  return rtp_rtcp_modules_.erase(local_ssrc) == 1 ? OK : NAME_NOT_FOUND;
}

rtp_rtcp::RtpRtcpInterface* RtpSession::GetRtpRtcpModule(
    uint32_t local_ssrc) const {
  std::scoped_lock lock(mutex_);
  const auto module_it = rtp_rtcp_modules_.find(local_ssrc);
  return module_it == rtp_rtcp_modules_.end() ? nullptr
                                              : module_it->second.get();
}

rtp_rtcp::Transport* RtpSession::outgoing_transport() const {
  return transport_.get();
}

rtp_rtcp::ReceiveStatistics* RtpSession::receive_statistics() const {
  return receive_statistics_.get();
}

RtpSessionStats RtpSession::GetStats() const {
  std::scoped_lock lock(mutex_);
  RtpSessionStats result = stats_;
  result.receive_streams.reserve(receive_streams_.size());
  for (const auto& [stream_id, entry] : receive_streams_) {
    static_cast<void>(stream_id);
    RtpReceiveStreamStats stream_stats = entry.stats;
    std::ranges::sort(stream_stats.remote_ssrcs);
    result.receive_streams.push_back(std::move(stream_stats));
  }
  std::ranges::sort(result.receive_streams, {},
                    &RtpReceiveStreamStats::stream_id);
  return result;
}

void RtpSession::OnPacketReceived(std::span<const uint8_t> packet,
                                  const PacketEndpoint& source,
                                  base::Timestamp arrival_time) {
  if (IsRtcpMuxPacket(packet)) {
    HandleRtcpPacket(packet, source);
  } else {
    HandleRtpPacket(packet, source, arrival_time);
  }
}

void RtpSession::HandleRtpPacket(std::span<const uint8_t> packet,
                                 const PacketEndpoint& source,
                                 base::Timestamp arrival_time) {
  rtp_rtcp::RtpPacketReceived rtp_packet(&extension_map_, arrival_time);
  if (!rtp_packet.Parse(packet)) {
    {
      std::scoped_lock lock(mutex_);
      ++stats_.invalid_packets;
    }
    NotifyPacketDiscarded(RtpSessionDiscardReason::kInvalidRtpPacket, packet,
                          source);
    return;
  }

  std::shared_ptr<rtp_rtcp::RtpPacketSinkInterface> sink;
  bool allow_stream_factory = false;
  {
    std::scoped_lock lock(mutex_);
    std::optional<RtpReceiveStreamId> stream_id;

    const std::optional<std::string> mid =
        rtp_packet.GetExtension<rtp_rtcp::RtpMid>();
    if (mid.has_value()) {
      const auto mid_it = mid_routes_.find(*mid);
      if (mid_it != mid_routes_.end()) {
        stream_id = mid_it->second;
      }
    }

    if (!stream_id.has_value()) {
      const auto ssrc_it = ssrc_routes_.find(rtp_packet.Ssrc());
      if (ssrc_it != ssrc_routes_.end()) {
        stream_id = ssrc_it->second;
      }
    }

    if (!stream_id.has_value()) {
      std::optional<RtpReceiveStreamId> payload_type_match;
      for (const auto& [candidate_id, entry] : receive_streams_) {
        if (!entry.config.allow_payload_type_discovery ||
            !entry.stats.remote_ssrcs.empty() ||
            !ContainsPayloadType(entry.config, rtp_packet.PayloadType())) {
          continue;
        }
        if (payload_type_match.has_value()) {
          payload_type_match.reset();
          break;
        }
        payload_type_match = candidate_id;
      }
      if (payload_type_match.has_value()) {
        stream_id = payload_type_match;
        ssrc_routes_.emplace(rtp_packet.Ssrc(), *stream_id);
      }
    }

    if (!stream_id.has_value()) {
      allow_stream_factory = true;
    } else {
      ReceiveStreamEntry& entry = receive_streams_.at(*stream_id);
      if (!ContainsPayloadType(entry.config, rtp_packet.PayloadType())) {
        stream_id.reset();
      } else {
        if (std::ranges::find(entry.stats.remote_ssrcs, rtp_packet.Ssrc()) ==
            entry.stats.remote_ssrcs.end()) {
          entry.stats.remote_ssrcs.push_back(rtp_packet.Ssrc());
          ssrc_routes_.try_emplace(rtp_packet.Ssrc(), *stream_id);
        }
        ++stats_.rtp_packets_received;
        ++entry.stats.packets_received;
        entry.stats.bytes_received += packet.size();
        entry.stats.payload_bytes_received += rtp_packet.payload_size();
        sink = entry.sink;
        rtp_packet.set_payload_type_frequency(
            static_cast<int32_t>(entry.config.clock_rate_hz));
      }
    }
  }

  if (sink == nullptr) {
    if (allow_stream_factory && receive_stream_factory_) {
      std::optional<DiscoveredRtpReceiveStream> discovered_stream =
          receive_stream_factory_(rtp_packet, source);
      if (discovered_stream.has_value() && discovered_stream->sink != nullptr &&
          ContainsPayloadType(discovered_stream->config,
                              rtp_packet.PayloadType())) {
        discovered_stream->config.remote_ssrc = rtp_packet.Ssrc();
        RtpReceiveStreamId stream_id = 0;
        const status_t add_status =
            AddReceiveStream(std::move(discovered_stream->config),
                             std::move(discovered_stream->sink), &stream_id);
        bool route_exists = add_status == OK;
        if (add_status == ALREADY_EXISTS) {
          std::scoped_lock lock(mutex_);
          route_exists = ssrc_routes_.contains(rtp_packet.Ssrc());
        }
        if (route_exists) {
          HandleRtpPacket(packet, source, arrival_time);
          return;
        }
      }
    }
    {
      std::scoped_lock lock(mutex_);
      ++stats_.unmatched_rtp_packets;
    }
    NotifyPacketDiscarded(RtpSessionDiscardReason::kUnmatchedRtpPacket, packet,
                          source);
    return;
  }
  receive_statistics_->OnRtpPacket(rtp_packet);
  transport_->AddRemoteRtcpEndpoint(source);
  if (observer_ != nullptr) {
    observer_->OnRtpPacketReceived(rtp_packet, source);
  }
  sink->OnRtpPacket(rtp_packet);
}

void RtpSession::HandleRtcpPacket(std::span<const uint8_t> packet,
                                  const PacketEndpoint& source) {
  if (!ValidateRtcpPacket(packet)) {
    {
      std::scoped_lock lock(mutex_);
      ++stats_.invalid_packets;
    }
    NotifyPacketDiscarded(RtpSessionDiscardReason::kInvalidRtcpPacket, packet,
                          source);
    return;
  }

  std::vector<rtp_rtcp::RtpRtcpInterface*> modules;
  {
    std::scoped_lock lock(mutex_);
    ++stats_.rtcp_packets_received;
    modules.reserve(rtp_rtcp_modules_.size());
    for (const auto& [local_ssrc, module] : rtp_rtcp_modules_) {
      static_cast<void>(local_ssrc);
      modules.push_back(module.get());
    }
  }
  if (observer_ != nullptr) {
    observer_->OnRtcpPacketReceived(packet, source);
  }
  // Receive streams and their RTCP modules cannot be changed while the
  // session is running, so the snapshot remains valid. Do not invoke RTP/RTCP
  // code while holding the session mutex: feedback callbacks may re-enter the
  // session.
  for (rtp_rtcp::RtpRtcpInterface* module : modules) {
    module->IncomingRtcpPacket(packet);
  }
}

void RtpSession::NotifyPacketDiscarded(RtpSessionDiscardReason reason,
                                       std::span<const uint8_t> packet,
                                       const PacketEndpoint& source) const {
  if (observer_ != nullptr) {
    observer_->OnSessionPacketDiscarded(reason, packet, source);
  }
}

bool RtpSession::ValidateRtcpPacket(std::span<const uint8_t> packet) {
  size_t offset = 0;
  while (offset < packet.size()) {
    rtp_rtcp::rtcp::CommonHeader header;
    if (!header.Parse(packet.data() + offset, packet.size() - offset) ||
        header.packet_size() == 0) {
      return false;
    }
    offset += header.packet_size();
  }
  return offset == packet.size();
}

}  // namespace ave::media::rtp_session
