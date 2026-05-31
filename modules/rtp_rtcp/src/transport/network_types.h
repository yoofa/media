/*
 * network_types.h - Network transport types
 *
 * Copyright (c) 2025 The AveStart project authors. All Rights Reserved.
 *
 * Ported from WebRTC project.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_TRANSPORT_NETWORK_TYPES_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_TRANSPORT_NETWORK_TYPES_H_

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "base/net/ecn_marking.h"
#include "base/units/data_rate.h"
#include "base/units/data_size.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

using DataRate = ::ave::base::DataRate;
using DataSize = ::ave::base::DataSize;
using EcnMarking = ::ave::base::EcnMarking;
using TimeDelta = ::ave::base::TimeDelta;
using Timestamp = ::ave::base::Timestamp;

// Configuration

// Represents constraints and rates related to the currently enabled streams.
struct BitrateAllocationLimits {
  // The total minimum send bitrate required by all sending streams.
  DataRate min_allocatable_rate = DataRate::Zero();
  // The total maximum allocatable bitrate for all currently available streams.
  DataRate max_allocatable_rate = DataRate::Zero();
  // The max bitrate to use for padding. The sum of the per-stream max padding
  // rate.
  DataRate max_padding_rate = DataRate::Zero();
};

// Use StreamsConfig for information about streams that is required for specific
// adjustments to the algorithms in network controllers.
struct StreamsConfig {
  StreamsConfig() = default;
  StreamsConfig(const StreamsConfig&) = default;
  ~StreamsConfig() = default;
  Timestamp at_time = Timestamp::PlusInfinity();
  std::optional<bool> requests_alr_probing;
  std::optional<bool> enable_repeated_initial_probing;
  std::optional<double> pacing_factor;
  std::optional<DataRate> min_total_allocated_bitrate;
  std::optional<DataRate> max_padding_rate;
  std::optional<DataRate> max_total_allocated_bitrate;
};

struct TargetRateConstraints {
  TargetRateConstraints() = default;
  TargetRateConstraints(const TargetRateConstraints&) = default;
  ~TargetRateConstraints() = default;
  Timestamp at_time = Timestamp::PlusInfinity();
  std::optional<DataRate> min_data_rate;
  std::optional<DataRate> max_data_rate;
  std::optional<DataRate> starting_rate;
};

// Send side information

struct NetworkAvailability {
  Timestamp at_time = Timestamp::PlusInfinity();
  bool network_available = false;
};

struct NetworkRouteChange {
  NetworkRouteChange() = default;
  NetworkRouteChange(const NetworkRouteChange&) = default;
  ~NetworkRouteChange() = default;
  Timestamp at_time = Timestamp::PlusInfinity();
  TargetRateConstraints constraints;
};

struct PacedPacketInfo {
  PacedPacketInfo() = default;
  PacedPacketInfo(int probe_cluster_id,
                  int probe_cluster_min_probes,
                  int probe_cluster_min_bytes)
      : probe_cluster_id(probe_cluster_id),
        probe_cluster_min_probes(probe_cluster_min_probes),
        probe_cluster_min_bytes(probe_cluster_min_bytes) {}

  bool operator==(const PacedPacketInfo& rhs) const {
    return send_bitrate == rhs.send_bitrate &&
           probe_cluster_id == rhs.probe_cluster_id &&
           probe_cluster_min_probes == rhs.probe_cluster_min_probes &&
           probe_cluster_min_bytes == rhs.probe_cluster_min_bytes &&
           probe_cluster_bytes_sent == rhs.probe_cluster_bytes_sent;
  }

  static constexpr int kNotAProbe = -1;
  DataRate send_bitrate = DataRate::BitsPerSec(0);
  int probe_cluster_id = kNotAProbe;
  int probe_cluster_min_probes = -1;
  int probe_cluster_min_bytes = -1;
  int probe_cluster_bytes_sent = 0;
};

struct SentPacket {
  Timestamp send_time = Timestamp::PlusInfinity();
  DataSize size = DataSize::Zero();
  DataSize prior_unacked_data = DataSize::Zero();
  PacedPacketInfo pacing_info;
  bool audio = false;
  int64_t sequence_number = 0;
  DataSize data_in_flight = DataSize::Zero();
};

struct ReceivedPacket {
  Timestamp send_time = Timestamp::MinusInfinity();
  Timestamp receive_time = Timestamp::PlusInfinity();
  DataSize size = DataSize::Zero();
};

// Transport level feedback

struct RemoteBitrateReport {
  Timestamp receive_time = Timestamp::PlusInfinity();
  DataRate bandwidth = DataRate::Infinity();
};

struct RoundTripTimeUpdate {
  Timestamp receive_time = Timestamp::PlusInfinity();
  TimeDelta round_trip_time = TimeDelta::PlusInfinity();
  bool smoothed = false;
};

struct TransportLossReport {
  Timestamp receive_time = Timestamp::PlusInfinity();
  Timestamp start_time = Timestamp::PlusInfinity();
  Timestamp end_time = Timestamp::PlusInfinity();
  uint64_t packets_lost_delta = 0;
  uint64_t packets_received_delta = 0;
};

// Packet level feedback

struct PacketResult {
  class ReceiveTimeOrder {
   public:
    bool operator()(const PacketResult& lhs, const PacketResult& rhs) {
      return lhs.receive_time < rhs.receive_time;
    }
  };

  PacketResult() = default;
  PacketResult(const PacketResult&) = default;
  ~PacketResult() = default;

  inline bool IsReceived() const { return !receive_time.IsPlusInfinity(); }

  SentPacket sent_packet;
  Timestamp receive_time = Timestamp::PlusInfinity();
  EcnMarking ecn = EcnMarking::kNotEct;
};

struct TransportPacketsFeedback {
  TransportPacketsFeedback() = default;
  TransportPacketsFeedback(const TransportPacketsFeedback& other) = default;
  ~TransportPacketsFeedback() = default;

  Timestamp feedback_time = Timestamp::PlusInfinity();
  DataSize data_in_flight = DataSize::Zero();
  bool transport_supports_ecn = false;
  std::vector<PacketResult> packet_feedbacks;
  std::vector<Timestamp> sendless_arrival_times;

  std::vector<PacketResult> ReceivedWithSendInfo() const {
    std::vector<PacketResult> result;
    for (const auto& pf : packet_feedbacks) {
      if (pf.IsReceived()) {
        result.push_back(pf);
      }
    }
    return result;
  }

  std::vector<PacketResult> LostWithSendInfo() const {
    std::vector<PacketResult> result;
    for (const auto& pf : packet_feedbacks) {
      if (!pf.IsReceived()) {
        result.push_back(pf);
      }
    }
    return result;
  }

  std::vector<PacketResult> PacketsWithFeedback() const {
    return packet_feedbacks;
  }

  std::vector<PacketResult> SortedByReceiveTime() const {
    std::vector<PacketResult> result = packet_feedbacks;
    std::sort(result.begin(), result.end(), PacketResult::ReceiveTimeOrder());
    return result;
  }
};

// Network estimation

struct NetworkEstimate {
  Timestamp at_time = Timestamp::PlusInfinity();
  DataRate bandwidth = DataRate::Infinity();
  TimeDelta round_trip_time = TimeDelta::PlusInfinity();
  TimeDelta bwe_period = TimeDelta::PlusInfinity();
  float loss_rate_ratio = 0;
};

// Network control

struct PacerConfig {
  Timestamp at_time = Timestamp::PlusInfinity();
  DataSize data_window = DataSize::Infinity();
  TimeDelta time_window = TimeDelta::PlusInfinity();
  DataSize pad_window = DataSize::Zero();
  DataRate data_rate() const { return data_window / time_window; }
  DataRate pad_rate() const { return pad_window / time_window; }
};

struct ProbeClusterConfig {
  Timestamp at_time = Timestamp::PlusInfinity();
  DataRate target_data_rate = DataRate::Zero();
  TimeDelta target_duration = TimeDelta::Zero();
  TimeDelta min_probe_delta = TimeDelta::Millis(2);
  int32_t target_probe_count = 0;
  int32_t id = 0;
};

struct TargetTransferRate {
  Timestamp at_time = Timestamp::PlusInfinity();
  NetworkEstimate network_estimate;
  DataRate target_rate = DataRate::Zero();
  DataRate stable_target_rate = DataRate::Zero();
  double cwnd_reduce_ratio = 0;
};

struct NetworkControlUpdate {
  NetworkControlUpdate() = default;
  NetworkControlUpdate(const NetworkControlUpdate&) = default;
  ~NetworkControlUpdate() = default;

  bool has_updates() const {
    return congestion_window.has_value() || pacer_config.has_value() ||
           !probe_cluster_configs.empty() || target_rate.has_value();
  }

  std::optional<DataSize> congestion_window;
  std::optional<PacerConfig> pacer_config;
  std::vector<ProbeClusterConfig> probe_cluster_configs;
  std::optional<TargetTransferRate> target_rate;
};

// Process control
struct ProcessInterval {
  Timestamp at_time = Timestamp::PlusInfinity();
  std::optional<DataSize> pacer_queue;
};

// Network state estimate
struct NetworkStateEstimate {
  double confidence = NAN;
  Timestamp update_time = Timestamp::MinusInfinity();
  Timestamp last_receive_time = Timestamp::MinusInfinity();
  Timestamp last_send_time = Timestamp::MinusInfinity();
  DataRate link_capacity = DataRate::MinusInfinity();
  DataRate link_capacity_lower = DataRate::MinusInfinity();
  DataRate link_capacity_upper = DataRate::MinusInfinity();
  TimeDelta pre_link_buffer_delay = TimeDelta::MinusInfinity();
  TimeDelta post_link_buffer_delay = TimeDelta::MinusInfinity();
  TimeDelta propagation_delay = TimeDelta::MinusInfinity();
  TimeDelta time_delta = TimeDelta::MinusInfinity();
  Timestamp last_feed_time = Timestamp::MinusInfinity();
  double cross_delay_rate = NAN;
  double spike_delay_rate = NAN;
  DataRate link_capacity_std_dev = DataRate::MinusInfinity();
  DataRate link_capacity_min = DataRate::MinusInfinity();
  double cross_traffic_ratio = NAN;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_TRANSPORT_NETWORK_TYPES_H_
