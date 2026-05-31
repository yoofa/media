/*
 * rtp_rtcp_interface.h - RTP/RTCP module interface
 *
 * Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-oriented framework.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_INTERFACE_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_INTERFACE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <memory>
#include <span>

#include "base/task_util/task_runner_base.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/foundation/video_bitrate_allocation.h"
#include "media/modules/include/module_fec_types.h"
#include "media/modules/rtp_rtcp/api/receive_statistics.h"
#include "media/modules/rtp_rtcp/api/report_block_data.h"
#include "media/modules/rtp_rtcp/api/rtcp_statistics.h"
#include "media/modules/rtp_rtcp/api/rtp_headers.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sender.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/fec/video_fec_generator.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sequence_number_map.h"
#include "media/modules/rtp_rtcp/src/transport/network_types.h"
#include "media/modules/rtp_rtcp/src/transport/transport.h"
#include "media/modules/rtp_rtcp/src/util/ntp_time.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// All types are now in the same namespace (ave::media::rtp_rtcp)

// Forward declarations.
class FrameEncryptorInterface;
class FrameTransformerInterface;
class RateLimiter;
class RTPSender;

class RtpRtcpInterface : public RtcpFeedbackSenderInterface {
 public:
  struct Configuration {
    // True for a audio version of the RTP/RTCP module object false will create
    // a video version.
    bool audio = false;
    bool receiver_only = false;

    ReceiveStatisticsProvider* receive_statistics = nullptr;

    // Transport object that will be called when packets are ready to be sent
    // out on the network.
    Transport* outgoing_transport = nullptr;

    // Called when the receiver requests an intra frame.
    RtcpIntraFrameObserver* intra_frame_callback = nullptr;

    // Called when the receiver sends a loss notification.
    RtcpLossNotificationObserver* rtcp_loss_notification_observer = nullptr;

    // Called when receive an RTCP message related to the link in general, e.g.
    // bandwidth estimation related message.
    NetworkLinkRtcpObserver* network_link_rtcp_observer = nullptr;

    NetworkStateEstimateObserver* network_state_estimate_observer = nullptr;

    TransportFeedbackObserver* transport_feedback_callback = nullptr;
    VideoBitrateAllocationObserver* bitrate_allocation_observer = nullptr;
    RtcpRttStats* rtt_stats = nullptr;
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer = nullptr;
    // Called on receipt of RTCP report block from remote side.
    RtcpCnameCallback* rtcp_cname_callback = nullptr;
    ReportBlockDataObserver* report_block_data_observer = nullptr;

    // Spread any bursts of packets into smaller bursts to minimize packet loss.
    RtpPacketSender* paced_sender = nullptr;

    // Generates FEC packets.
    VideoFecGenerator* fec_generator = nullptr;

    BitrateStatisticsObserver* send_bitrate_observer = nullptr;
    SendPacketObserver* send_packet_observer = nullptr;
    RateLimiter* retransmission_rate_limiter = nullptr;
    StreamDataCountersCallback* rtp_stats_callback = nullptr;

    int rtcp_report_interval_ms = 0;

    // Update network2 instead of pacer_exit field of video timing extension.
    bool populate_network2_timestamp = false;

    std::shared_ptr<FrameTransformerInterface> frame_transformer;

    // E2EE Custom Video Frame Encryption
    FrameEncryptorInterface* frame_encryptor = nullptr;
    // Require all outgoing frames to be encrypted with a FrameEncryptor.
    bool require_frame_encryption = false;

    // Corresponds to extmap-allow-mixed in SDP negotiation.
    bool extmap_allow_mixed = false;

    // If true, the RTP sender will always annotate outgoing packets with
    // MID and RID header extensions, if provided and negotiated.
    // If false, the RTP sender will stop sending MID and RID header extensions,
    // when it knows that the receiver is ready to demux based on SSRC.
    bool always_send_mid_and_rid = false;

    // SSRCs for media and retransmission, respectively.
    uint32_t local_media_ssrc = 0;
    std::optional<uint32_t> rtx_send_ssrc;

    bool need_rtp_packet_infos = false;

    // Estimate RTT as non-sender as described in
    // https://tools.ietf.org/html/rfc3611#section-4.4 and #section-4.5
    bool non_sender_rtt_measurement = false;

    // If non-empty, sets the value for sending in the RID (and Repaired) RTP
    // header extension.
    std::string rid;

    // Enables send packet batching from the egress RTP sender.
    bool enable_send_packet_batching = false;

    // Task runner for async operations.
    base::TaskRunnerBase* task_runner = nullptr;
  };

  // Stats for RTCP sender reports (SR) for a specific SSRC.
  struct SenderReportStats {
    // Arrival timestamp (environment clock) for the last received RTCP SR.
    base::Timestamp last_arrival_timestamp = base::Timestamp::Zero();
    // Arrival NTP timestamp for the last received RTCP SR.
    NtpTime last_arrival_ntp_timestamp;
    // Received (a.k.a., remote) NTP timestamp for the last received RTCP SR.
    NtpTime last_remote_ntp_timestamp;
    // Received (a.k.a., remote) RTP timestamp from the last received RTCP SR.
    uint32_t last_remote_rtp_timestamp = 0;
    // Total number of RTP data packets transmitted by the sender since starting
    // transmission up until the time this SR packet was generated.
    uint32_t packets_sent = 0;
    // Total number of payload octets transmitted in RTP data packets by the
    // sender since starting transmission up until the time this SR packet was
    // generated.
    uint64_t bytes_sent = 0;
    // Total number of RTCP SR blocks received.
    uint64_t reports_count = 0;
  };

  // Stats about the non-sender SSRC, based on RTCP extended reports (XR).
  struct NonSenderRttStats {
    std::optional<base::TimeDelta> round_trip_time;
    base::TimeDelta total_round_trip_time = base::TimeDelta::Zero();
    int round_trip_time_measurements = 0;
  };

  // Receiver functions
  virtual void IncomingRtcpPacket(std::span<const uint8_t> incoming_packet) = 0;

  virtual void SetRemoteSSRC(uint32_t ssrc) = 0;

  // Called when the local ssrc changes (post initialization) for receive
  // streams to match with send.
  virtual void SetLocalSsrc(uint32_t ssrc) = 0;

  // Sender functions

  // Sets the maximum size of an RTP packet, including RTP headers.
  virtual void SetMaxRtpPacketSize(size_t size) = 0;

  // Returns max RTP packet size.
  virtual size_t MaxRtpPacketSize() const = 0;

  virtual void RegisterSendPayloadFrequency(int payload_type,
                                            int payload_frequency) = 0;

  // Unregisters a send payload.
  virtual int32_t DeRegisterSendPayload(int8_t payload_type) = 0;

  virtual void SetExtmapAllowMixed(bool extmap_allow_mixed) = 0;

  // Register extension by uri, triggers CHECK on failure.
  virtual void RegisterRtpHeaderExtension(std::string_view uri, int id) = 0;

  virtual void DeregisterSendRtpHeaderExtension(std::string_view uri) = 0;

  // Returns true if RTP module is send media, and any of the extensions
  // required for bandwidth estimation is registered.
  virtual bool SupportsPadding() const = 0;
  virtual bool SupportsRtxPayloadPadding() const = 0;

  // Returns start timestamp.
  virtual uint32_t StartTimestamp() const = 0;

  // Sets start timestamp.
  virtual void SetStartTimestamp(uint32_t timestamp) = 0;

  // Returns SequenceNumber.
  virtual uint16_t SequenceNumber() const = 0;

  // Sets SequenceNumber, default is a random number.
  virtual void SetSequenceNumber(uint16_t seq) = 0;

  virtual void SetRtpState(const RtpState& rtp_state) = 0;
  virtual void SetRtxState(const RtpState& rtp_state) = 0;
  virtual RtpState GetRtpState() const = 0;
  virtual RtpState GetRtxState() const = 0;

  // This can be used to enable/disable receive-side RTT.
  virtual void SetNonSenderRttMeasurement(bool enabled) = 0;

  // Returns SSRC.
  virtual uint32_t SSRC() const = 0;

  // Sets the value for sending in the MID RTP header extension.
  virtual void SetMid(std::string_view mid) = 0;

  // Turns on/off sending RTX (RFC 4588).
  virtual void SetRtxSendStatus(int modes) = 0;

  // Returns status of sending RTX (RFC 4588).
  virtual int RtxSendStatus() const = 0;

  // Returns the SSRC used for RTX if set, otherwise a nullopt.
  virtual std::optional<uint32_t> RtxSsrc() const = 0;

  // Sets the payload type to use when sending RTX packets.
  virtual void SetRtxSendPayloadType(int payload_type,
                                     int associated_payload_type) = 0;

  // Returns the FlexFEC SSRC, if there is one.
  virtual std::optional<uint32_t> FlexfecSsrc() const = 0;

  // Sets sending status.
  virtual int32_t SetSendingStatus(bool sending) = 0;

  // Returns current sending status.
  virtual bool Sending() const = 0;

  // Starts/Stops media packets.
  virtual void SetSendingMediaStatus(bool sending) = 0;

  // Returns current media sending status.
  virtual bool SendingMedia() const = 0;

  // Returns whether audio is configured.
  virtual bool IsAudioConfigured() const = 0;

  // Indicate that the packets sent by this module should be counted towards the
  // bitrate estimate since the stream participates in the bitrate allocation.
  virtual void SetAsPartOfAllocation(bool part_of_allocation) = 0;

  // Returns bitrate sent (post-pacing) per packet type.
  virtual RtpSendRates GetSendRates() const = 0;

  virtual RTPSender* RtpSender() = 0;
  virtual const RTPSender* RtpSender() const = 0;

  // Record that a frame is about to be sent.
  virtual bool OnSendingRtpFrame(uint32_t timestamp,
                                 int64_t capture_time_ms,
                                 int payload_type,
                                 bool force_sender_report) = 0;

  // Try to send the provided packet.
  virtual bool TrySendPacket(std::unique_ptr<RtpPacketToSend> packet,
                             const PacedPacketInfo& pacing_info) = 0;

  virtual bool CanSendPacket(const RtpPacketToSend& packet) const = 0;

  virtual void AssignSequenceNumber(RtpPacketToSend& packet) = 0;

  virtual void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& pacing_info) = 0;

  // Notifies that a batch of packet sends is completed.
  virtual void OnBatchComplete() = 0;

  // Update the FEC protection parameters to use for delta- and key-frames.
  virtual void SetFecProtectionParams(
      const FecProtectionParams& delta_params,
      const FecProtectionParams& key_params) = 0;

  // If deferred FEC generation is enabled, returns generated FEC packets.
  virtual std::vector<std::unique_ptr<RtpPacketToSend>> FetchFecPackets() = 0;

  virtual void OnAbortedRetransmissions(
      std::span<const uint16_t> sequence_numbers) = 0;

  virtual void OnPacketsAcknowledged(
      std::span<const uint16_t> sequence_numbers) = 0;

  virtual std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) = 0;

  virtual std::vector<RtpSequenceNumberMap::Info> GetSentRtpPacketInfos(
      std::span<const uint16_t> sequence_numbers) const = 0;

  // Returns an expected per packet overhead.
  virtual size_t ExpectedPerPacketOverhead() const = 0;

  virtual void OnPacketSendingThreadSwitched() = 0;

  // RTCP functions

  // Returns RTCP status.
  virtual RtcpMode RTCP() const = 0;

  // Sets RTCP status.
  virtual void SetRTCPStatus(RtcpMode method) = 0;

  // Sets RTCP CName.
  virtual int32_t SetCNAME(std::string_view cname) = 0;

  // Returns current RTT estimate.
  virtual std::optional<base::TimeDelta> LastRtt() const = 0;

  // Returns the estimated RTT, with fallback to a default value.
  virtual base::TimeDelta ExpectedRetransmissionTime() const = 0;

  // Forces a send of a RTCP packet.
  virtual int32_t SendRTCP(RTCPPacketType rtcp_packet_type) = 0;

  // Returns send statistics for the RTP and RTX stream.
  virtual void GetSendStreamDataCounters(
      StreamDataCounters* rtp_counters,
      StreamDataCounters* rtx_counters) const = 0;

  // Returns latest report block data.
  virtual std::vector<ReportBlockData> GetLatestReportBlockData() const = 0;

  // Returns stats based on the received RTCP SRs.
  virtual std::optional<SenderReportStats> GetSenderReportStats() const = 0;

  // Returns non-sender RTT stats, based on DLRR.
  virtual std::optional<NonSenderRttStats> GetNonSenderRttStats() const = 0;

  // (REMB) Receiver Estimated Max Bitrate.
  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) override = 0;
  void UnsetRemb() override = 0;

  // (NACK)
  virtual int32_t SendNACK(const uint16_t* nack_list, uint16_t size) = 0;

  virtual void SendNack(const std::vector<uint16_t>& sequence_numbers) = 0;

  // Store the sent packets, needed to answer to NACK requests.
  virtual void SetStorePacketsStatus(bool enable, uint16_t numberToStore) = 0;

  virtual void SetVideoBitrateAllocation(
      const VideoBitrateAllocation& bitrate) = 0;

  // Video functions

  // Requests new key frame.
  void SendPictureLossIndication() { SendRTCP(kRtcpPli); }
  void SendFullIntraRequest() { SendRTCP(kRtcpFir); }

  // Sends a LossNotification RTCP message.
  virtual int32_t SendLossNotification(uint16_t last_decoded_seq_num,
                                       uint16_t last_received_seq_num,
                                       bool decodability_flag,
                                       bool buffering_allowed) = 0;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_INTERFACE_H_
