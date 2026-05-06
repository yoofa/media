/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_
#define AVE_MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <span>
#include "base/units/data_rate.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_parameters.h"

namespace ave {
namespace media {

// Forward declaration for VideoBitrateAllocation - it's defined in
// media/foundation/video_bitrate_allocation.h
class VideoBitrateAllocation;

namespace rtp_rtcp {

using TimeDelta = ::ave::base::TimeDelta;
using Timestamp = ::ave::base::Timestamp;
using DataRate = ::ave::base::DataRate;

// RFC 3550 page 44, including null termination
constexpr size_t kRtcpCnameSize = 256;
// We assume ethernet
constexpr size_t kIpPacketSize = 1500;

// For backward compatibility
constexpr size_t IP_PACKET_SIZE = 1500;

constexpr int32_t kVideoPayloadTypeFrequency = 90000;

// TODO(bugs.webrtc.org/6458): Remove this when all the depending projects are
// updated to correctly set rtp rate for RtcpSender.
constexpr int32_t kBogusRtpRateForAudioRtcp = 8000;

// Minimum RTP header size in bytes.
constexpr uint8_t kRtpHeaderSize = 12;

bool IsLegalMidName(std::string_view name);
bool IsLegalRsidName(std::string_view name);

// This enum must not have any gaps, i.e., all integers between
// kRtpExtensionNone and kRtpExtensionNumberOfExtensions must be valid enum
// entries.
enum RTPExtensionType : int32_t {
  kRtpExtensionNone,
  kRtpExtensionTransmissionTimeOffset,
  kRtpExtensionAudioLevel,
  kRtpExtensionCsrcAudioLevel,
  kRtpExtensionInbandComfortNoise,
  kRtpExtensionAbsoluteSendTime,
  kRtpExtensionAbsoluteCaptureTime,
  kRtpExtensionVideoRotation,
  kRtpExtensionTransportSequenceNumber,
  kRtpExtensionTransportSequenceNumber02,
  kRtpExtensionPlayoutDelay,
  kRtpExtensionVideoContentType,
  kRtpExtensionVideoLayersAllocation,
  kRtpExtensionVideoTiming,
  kRtpExtensionRtpStreamId,
  kRtpExtensionRepairedRtpStreamId,
  kRtpExtensionMid,
  kRtpExtensionGenericFrameDescriptor,
  kRtpExtensionDependencyDescriptor,
  kRtpExtensionColorSpace,
  kRtpExtensionVideoFrameTrackingId,
  kRtpExtensionCorruptionDetection,
  kRtpExtensionNumberOfExtensions  // Must be the last entity in the enum.
};

enum RTCPAppSubTypes { kAppSubtypeBwe = 0x00 };

// TODO(sprang): Make this an enum class once rtcp_receiver has been cleaned up.
enum RTCPPacketType : uint32_t {
  kRtcpReport = 0x0001,
  kRtcpSr = 0x0002,
  kRtcpRr = 0x0004,
  kRtcpSdes = 0x0008,
  kRtcpBye = 0x0010,
  kRtcpPli = 0x0020,
  kRtcpNack = 0x0040,
  kRtcpFir = 0x0080,
  kRtcpTmmbr = 0x0100,
  kRtcpTmmbn = 0x0200,
  kRtcpSrReq = 0x0400,
  kRtcpLossNotification = 0x2000,
  kRtcpRemb = 0x10000,
  kRtcpTransmissionTimeOffset = 0x20000,
  kRtcpXrReceiverReferenceTime = 0x40000,
  kRtcpXrDlrrReportBlock = 0x80000,
  kRtcpTransportFeedback = 0x100000,
  kRtcpXrTargetBitrate = 0x200000,
  kRtcpCcFeedback = 0x400000,
};

enum class KeyFrameReqMethod : uint8_t {
  kNone,     // Don't request keyframes.
  kPliRtcp,  // Request keyframes through Picture Loss Indication.
  kFirRtcp   // Request keyframes through Full Intra-frame Request.
};

// RTCP mode to use. Compound mode is described by RFC 4585 and reduced-size
// RTCP mode is described by RFC 5506.
enum class RtcpMode { kOff, kCompound, kReducedSize };

enum RtxMode {
  kRtxOff = 0x0,
  kRtxRetransmitted = 0x1,     // Only send retransmissions over RTX.
  kRtxRedundantPayloads = 0x2  // Preventively send redundant payloads
                               // instead of padding.
};

constexpr size_t kRtxHeaderSize = 2;

// RTP state structure.
struct RtpState {
  uint16_t sequence_number = 0;
  uint32_t start_timestamp = 0;
  uint32_t timestamp = 0;
  base::Timestamp capture_time = base::Timestamp::MinusInfinity();
  base::Timestamp last_timestamp_time = base::Timestamp::MinusInfinity();
  bool ssrc_has_acked = false;
};

// NOTE! `kNumMediaTypes` must be kept in sync with RtpPacketMediaType!
static constexpr size_t kNumMediaTypes = 5;
enum class RtpPacketMediaType : size_t {
  kAudio,                         // Audio media packets.
  kVideo,                         // Video media packets.
  kRetransmission,                // Retransmissions, sent as response to NACK.
  kForwardErrorCorrection,        // FEC packets.
  kPadding = kNumMediaTypes - 1,  // RTX or plain padding sent to maintain BWE.
};

// Forward declarations
class RtpPacketToSend;
struct PacedPacketInfo;

// Struct with info about a sent RTP packet, used for transport feedback.
struct RtpPacketSendInfo {
  static RtpPacketSendInfo From(const RtpPacketToSend& packet,
                                const PacedPacketInfo& pacing_info);

  uint16_t transport_sequence_number = 0;
  std::optional<uint32_t> media_ssrc;
  uint16_t rtp_sequence_number = 0;  // Only valid if `media_ssrc` is set.
  uint32_t rtp_timestamp = 0;
  size_t length = 0;
  std::optional<RtpPacketMediaType> packet_type;
  // PacedPacketInfo is defined in transport/network_types.h
  // For now, we inline a simplified version here to avoid circular deps.
  struct PacingInfo {
    static constexpr int32_t kNotAProbe = -1;
    int32_t probe_cluster_id = kNotAProbe;
    int32_t probe_cluster_min_probes = -1;
    int32_t probe_cluster_min_bytes = -1;
    int32_t probe_cluster_bytes_sent = 0;
  } pacing_info;
};

class RtcpIntraFrameObserver {
 public:
  virtual ~RtcpIntraFrameObserver() {}

  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) = 0;
};

// Observer for incoming LossNotification RTCP messages.
// See the documentation of LossNotification for details.
class RtcpLossNotificationObserver {
 public:
  virtual ~RtcpLossNotificationObserver() = default;

  virtual void OnReceivedLossNotification(uint32_t ssrc,
                                          uint16_t seq_num_of_last_decodable,
                                          uint16_t seq_num_of_last_received,
                                          bool decodability_flag) = 0;
};

class RtcpRttStats {
 public:
  virtual void OnRttUpdate(int64_t rtt) = 0;

  virtual int64_t LastProcessedRtt() const = 0;

  virtual ~RtcpRttStats() {}
};

// Forward declaration for NetworkStateEstimate
struct NetworkStateEstimate;

class NetworkStateEstimateObserver {
 public:
  virtual void OnRemoteNetworkEstimate(NetworkStateEstimate estimate) = 0;
  virtual ~NetworkStateEstimateObserver() = default;
};

class TransportFeedbackObserver {
 public:
  virtual ~TransportFeedbackObserver() = default;

  virtual void OnAddPacket(const RtpPacketSendInfo& packet_info) = 0;
};

// Forward declaration for rtcp::RtcpPacket
namespace rtcp {
class RtcpPacket;
}  // namespace rtcp

// Interface for PacketRouter to send rtcp feedback on behalf of
// congestion controller.
class RtcpFeedbackSenderInterface {
 public:
  virtual ~RtcpFeedbackSenderInterface() = default;
  virtual void SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) = 0;
  virtual void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) = 0;
  virtual void UnsetRemb() = 0;
};

class StreamFeedbackObserver {
 public:
  struct StreamPacketInfo {
    bool received;
    std::optional<uint32_t> ssrc;
    uint16_t rtp_sequence_number;
    bool is_retransmission;
  };
  virtual ~StreamFeedbackObserver() = default;

  virtual void OnPacketFeedbackVector(
      std::vector<StreamPacketInfo> packet_feedback_vector) = 0;
};

class StreamFeedbackProvider {
 public:
  virtual void RegisterStreamFeedbackObserver(
      std::vector<uint32_t> ssrcs,
      StreamFeedbackObserver* observer) = 0;
  virtual void DeRegisterStreamFeedbackObserver(
      StreamFeedbackObserver* observer) = 0;
  virtual ~StreamFeedbackProvider() = default;
};

// Note: RtpExtension is defined in rtp_parameters.h

// Callback, used to notify an observer whenever new rates have been estimated.
class BitrateStatisticsObserver {
 public:
  virtual ~BitrateStatisticsObserver() {}

  virtual void Notify(uint32_t total_bitrate_bps,
                      uint32_t retransmit_bitrate_bps,
                      uint32_t ssrc) = 0;
};

// Forward declarations
class RtpPacket;
class RtpPacketToSend;
class RtpPacketReceived;

struct RtpPacketCounter {
  RtpPacketCounter()
      : header_bytes(0), payload_bytes(0), padding_bytes(0), packets(0) {}

  void Add(const RtpPacketCounter& other) {
    header_bytes += other.header_bytes;
    payload_bytes += other.payload_bytes;
    padding_bytes += other.padding_bytes;
    packets += other.packets;
    total_packet_delay += other.total_packet_delay;
  }

  bool operator==(const RtpPacketCounter& other) const {
    return header_bytes == other.header_bytes &&
           payload_bytes == other.payload_bytes &&
           padding_bytes == other.padding_bytes && packets == other.packets &&
           total_packet_delay == other.total_packet_delay;
  }

  // Not inlined, since use of RtpPacket would result in circular includes.
  void AddPacket(const RtpPacket& packet);
  void AddPacket(const RtpPacketToSend& packet_to_send);
  void AddPacket(const RtpPacketReceived& packet);

  size_t TotalBytes() const {
    return header_bytes + payload_bytes + padding_bytes;
  }

  size_t header_bytes;   // Number of bytes used by RTP headers.
  size_t payload_bytes;  // Payload bytes, excluding RTP headers and padding.
  size_t padding_bytes;  // Number of padding bytes.
  size_t packets;        // Number of packets.
  // The total delay of all `packets`. For RtpPacketToSend packets, this is
  // `time_in_send_queue()`. For receive packets, this is zero.
  base::TimeDelta total_packet_delay = base::TimeDelta::Zero();
};

// Data usage statistics for a (rtp) stream.
struct StreamDataCounters {
  StreamDataCounters() = default;

  void Add(const StreamDataCounters& other) {
    transmitted.Add(other.transmitted);
    retransmitted.Add(other.retransmitted);
    fec.Add(other.fec);
    if (other.first_packet_time < first_packet_time) {
      // Use oldest time (excluding unsed value represented as plus infinity.
      first_packet_time = other.first_packet_time;
    }
  }

  void MaybeSetFirstPacketTime(Timestamp now) {
    if (first_packet_time == Timestamp::PlusInfinity()) {
      first_packet_time = now;
    }
  }

  // Return time since first packet is send/received, or zero if such event
  // haven't happen.
  base::TimeDelta TimeSinceFirstPacket(Timestamp now) const {
    return first_packet_time == Timestamp::PlusInfinity()
               ? base::TimeDelta::Zero()
               : now - first_packet_time;
  }

  // Returns the number of bytes corresponding to the actual media payload
  // (i.e. RTP headers, padding, retransmissions and fec packets are excluded).
  // Note this function does not have meaning for an RTX stream.
  size_t MediaPayloadBytes() const {
    return transmitted.payload_bytes - retransmitted.payload_bytes -
           fec.payload_bytes;
  }

  // Time when first packet is sent/received.
  Timestamp first_packet_time = Timestamp::PlusInfinity();

  RtpPacketCounter transmitted;    // Number of transmitted packets/bytes.
  RtpPacketCounter retransmitted;  // Number of retransmitted packets/bytes.
  RtpPacketCounter fec;            // Number of redundancy packets/bytes.
};

// Information exposed through the GetStats api.
struct RtpReceiveStats {
  // `packets_lost` and `jitter` are defined by RFC 3550, and exposed in the
  // RTCReceivedRtpStreamStats dictionary, see
  // https://w3c.github.io/webrtc-stats/#receivedrtpstats-dict*
  int32_t packets_lost = 0;
  // Interarrival jitter in samples.
  uint32_t jitter = 0;
  // Interarrival jitter in time.
  base::TimeDelta interarrival_jitter = base::TimeDelta::Zero();
  // Timestamp and counters for the last received packet.
  std::optional<Timestamp> last_packet_received;
  RtpPacketCounter packet_counter;
};

// Callback, called whenever byte/packet counts have been updated.
class StreamDataCountersCallback {
 public:
  virtual ~StreamDataCountersCallback() = default;

  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) = 0;
};

// Callback, used to notify an observer whenever a packet is sent to the
// transport.
class SendPacketObserver {
 public:
  virtual ~SendPacketObserver() = default;
  virtual void OnSendPacket(std::optional<uint16_t> packet_id,
                            Timestamp capture_time,
                            uint32_t ssrc) = 0;
};

// Forward declaration for RTCP packet types
namespace rtcp {
class TransportFeedback;
class CongestionControlFeedback;
}  // namespace rtcp

// Forward declaration for ReportBlockData
class ReportBlockData;

// Observer for receiving RTCP messages related to the link.
class NetworkLinkRtcpObserver {
 public:
  virtual ~NetworkLinkRtcpObserver() = default;

  virtual void OnTransportFeedback(
      Timestamp /* receive_time */,
      const rtcp::TransportFeedback& /* feedback */) {}
  // RFC 8888 congestion control feedback.
  virtual void OnCongestionControlFeedback(
      Timestamp /* receive_time */,
      const rtcp::CongestionControlFeedback& /* feedback */) {}
  virtual void OnReceiverEstimatedMaxBitrate(Timestamp /* receive_time */,
                                             DataRate /* bitrate */) {}

  // Called on an RTCP packet with sender or receiver reports with non zero
  // report blocks. Report blocks are combined from all reports into one array.
  virtual void OnReport(Timestamp /* receive_time */,
                        std::span<const ReportBlockData> /* report_blocks */) {}
  virtual void OnRttUpdate(Timestamp /* receive_time */, TimeDelta /* rtt */) {}
};

// Observer for receiving video bitrate allocation updates.
class VideoBitrateAllocationObserver {
 public:
  virtual ~VideoBitrateAllocationObserver() = default;
  virtual void OnBitrateAllocationUpdated(
      const media::VideoBitrateAllocation& allocation) = 0;
};

// Note: ReportBlockDataObserver is defined in report_block_data.h

class RtpSendRates {
 public:
  RtpSendRates() : send_rates_(kNumMediaTypes, DataRate::Zero()) {}
  RtpSendRates(const RtpSendRates& rhs) = default;
  RtpSendRates& operator=(const RtpSendRates&) = default;

  DataRate& operator[](RtpPacketMediaType type) {
    return send_rates_[static_cast<size_t>(type)];
  }
  const DataRate& operator[](RtpPacketMediaType type) const {
    return send_rates_[static_cast<size_t>(type)];
  }
  DataRate Sum() const {
    DataRate sum = DataRate::Zero();
    for (const auto& rate : send_rates_) {
      sum = sum + rate;
    }
    return sum;
  }

 private:
  std::vector<DataRate> send_rates_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_
