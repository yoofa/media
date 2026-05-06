/*
 * rtp_rtcp_impl2.h - RTP/RTCP module implementation
 * Ported from WebRTC
 *
 * Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 * Copyright (c) 2026 The aspect-oriented-express Authors. All rights reserved.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL2_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL2_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include <span>
#include "base/clock.h"
#include "base/task_util/pending_task_flag.h"
#include "base/task_util/repeating_task.h"
#include "base/task_util/task_runner_base.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/foundation/video_bitrate_allocation.h"
#include "media/modules/include/module_fec_types.h"
#include "media/modules/rtp_rtcp/api/report_block_data.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/tmmb_item.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_receiver.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_sender.h"
#include "media/modules/rtp_rtcp/src/rtp/packet_sequencer.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_history.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sender.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sender_egress.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sequence_number_map.h"

namespace ave {

using base::Clock;
using base::TaskRunnerBase;
using base::TimeDelta;
using base::Timestamp;

namespace media {
namespace rtp_rtcp {

struct RTPVideoHeader;

class ModuleRtpRtcpImpl2 final : public RtpRtcpInterface,
                                 public RTCPReceiver::ModuleRtpRtcp {
 public:
  ModuleRtpRtcpImpl2(Clock* clock,
                     TaskRunnerBase* worker_queue,
                     const RtpRtcpInterface::Configuration& configuration);
  ~ModuleRtpRtcpImpl2() override;

  // Receiver part.

  // Called when we receive an RTCP packet.
  void IncomingRtcpPacket(std::span<const uint8_t> rtcp_packet) override;

  void SetRemoteSSRC(uint32_t ssrc) override;

  void SetLocalSsrc(uint32_t local_ssrc) override;

  // Sender part.
  void RegisterSendPayloadFrequency(int32_t payload_type,
                                    int32_t payload_frequency) override;

  int32_t DeRegisterSendPayload(int8_t payload_type) override;

  void SetExtmapAllowMixed(bool extmap_allow_mixed) override;

  void RegisterRtpHeaderExtension(std::string_view uri, int32_t id) override;
  void DeregisterSendRtpHeaderExtension(std::string_view uri) override;

  bool SupportsPadding() const override;
  bool SupportsRtxPayloadPadding() const override;

  // Get start timestamp.
  uint32_t StartTimestamp() const override;

  // Configure start timestamp, default is a random number.
  void SetStartTimestamp(uint32_t timestamp) override;

  uint16_t SequenceNumber() const override;

  // Set SequenceNumber, default is a random number.
  void SetSequenceNumber(uint16_t seq) override;

  void SetRtpState(const RtpState& rtp_state) override;
  void SetRtxState(const RtpState& rtp_state) override;
  RtpState GetRtpState() const override;
  RtpState GetRtxState() const override;

  void SetNonSenderRttMeasurement(bool enabled) override;

  uint32_t SSRC() const override { return rtcp_sender_.SSRC(); }

  // Semantically identical to `SSRC()` but must be called on the packet
  // delivery thread/tq and returns the ssrc that maps to
  // RtpRtcpInterface::Configuration::local_media_ssrc.
  uint32_t local_media_ssrc() const;

  void SetMid(std::string_view mid) override;

  RTCPSender::FeedbackState GetFeedbackState();

  void SetRtxSendStatus(int32_t mode) override;
  int32_t RtxSendStatus() const override;
  std::optional<uint32_t> RtxSsrc() const override;

  void SetRtxSendPayloadType(int32_t payload_type,
                             int32_t associated_payload_type) override;

  std::optional<uint32_t> FlexfecSsrc() const override;

  // Sends kRtcpByeCode when going from true to false.
  int32_t SetSendingStatus(bool sending) override;

  bool Sending() const override;

  // Drops or relays media packets.
  void SetSendingMediaStatus(bool sending) override;

  bool SendingMedia() const override;

  bool IsAudioConfigured() const override;

  void SetAsPartOfAllocation(bool part_of_allocation) override;

  bool OnSendingRtpFrame(uint32_t timestamp,
                         int64_t capture_time_ms,
                         int32_t payload_type,
                         bool force_sender_report) override;

  bool CanSendPacket(const RtpPacketToSend& packet) const override;

  void AssignSequenceNumber(RtpPacketToSend& packet) override;

  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& pacing_info) override;

  bool TrySendPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& pacing_info) override;
  void OnBatchComplete() override;

  void SetFecProtectionParams(const FecProtectionParams& delta_params,
                              const FecProtectionParams& key_params) override;

  std::vector<std::unique_ptr<RtpPacketToSend>> FetchFecPackets() override;

  void OnAbortedRetransmissions(
      std::span<const uint16_t> sequence_numbers) override;

  void OnPacketsAcknowledged(
      std::span<const uint16_t> sequence_numbers) override;

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) override;

  std::vector<RtpSequenceNumberMap::Info> GetSentRtpPacketInfos(
      std::span<const uint16_t> sequence_numbers) const override;

  size_t ExpectedPerPacketOverhead() const override;

  void OnPacketSendingThreadSwitched() override;

  // RTCP part.

  // Get RTCP status.
  RtcpMode RTCP() const override;

  // Configure RTCP status i.e on/off.
  void SetRTCPStatus(RtcpMode method) override;

  // Set RTCP CName.
  int32_t SetCNAME(std::string_view c_name) override;

  // Get RoundTripTime.
  std::optional<TimeDelta> LastRtt() const override;

  TimeDelta ExpectedRetransmissionTime() const override;

  // Force a send of an RTCP packet.
  // Normal SR and RR are triggered via the task queue that's current when this
  // object is created.
  int32_t SendRTCP(RTCPPacketType packet_type) override;

  void GetSendStreamDataCounters(
      StreamDataCounters* rtp_counters,
      StreamDataCounters* rtx_counters) const override;

  // A snapshot of the most recent Report Block with additional data of
  // interest to statistics. Used to implement RTCRemoteInboundRtpStreamStats.
  // Within this list, the `ReportBlockData::source_ssrc()`, which is the SSRC
  // of the corresponding outbound RTP stream, is unique.
  std::vector<ReportBlockData> GetLatestReportBlockData() const override;
  std::optional<SenderReportStats> GetSenderReportStats() const override;
  std::optional<NonSenderRttStats> GetNonSenderRttStats() const override;

  // (REMB) Receiver Estimated Max Bitrate.
  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) override;
  void UnsetRemb() override;

  void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) override;

  size_t MaxRtpPacketSize() const override;

  void SetMaxRtpPacketSize(size_t rtp_packet_size) override;

  // (NACK) Negative acknowledgment part.

  // Send a Negative acknowledgment packet.
  int32_t SendNACK(const uint16_t* nack_list, uint16_t size) override;

  void SendNack(const std::vector<uint16_t>& sequence_numbers) override;

  // Store the sent packets, needed to answer to a negative acknowledgment
  // requests.
  void SetStorePacketsStatus(bool enable, uint16_t number_to_store) override;

  void SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) override;

  // Video part.
  int32_t SendLossNotification(uint16_t last_decoded_seq_num,
                               uint16_t last_received_seq_num,
                               bool decodability_flag,
                               bool buffering_allowed) override;

  RtpSendRates GetSendRates() const override;

  void OnReceivedNack(
      const std::vector<uint16_t>& nack_sequence_numbers) override;
  void OnReceivedRtcpReportBlocks(
      std::span<const ReportBlockData> report_blocks) override;
  void OnRequestSendReport() override;

  void SetVideoBitrateAllocation(
      const VideoBitrateAllocation& bitrate) override;

  RTPSender* RtpSender() override;
  const RTPSender* RtpSender() const override;

 private:
  struct RtpSenderContext {
    explicit RtpSenderContext(Clock* clock,
                              TaskRunnerBase* worker_queue,
                              const RtpRtcpInterface::Configuration& config);
    // Storage of packets, for retransmissions and padding, if applicable.
    RtpPacketHistory packet_history;
    // Handles sequence number assignment and padding timestamp generation.
    PacketSequencer sequencer;
    // Handles final time timestamping/stats/etc and handover to Transport.
    RtpSenderEgress packet_sender;
    // If no paced sender configured, this class will be used to pass packets
    // from `packet_generator_` to `packet_sender_`.
    RtpSenderEgress::NonPacedPacketSender non_paced_sender;
    // Handles creation of RTP packets to be sent.
    RTPSender packet_generator;
  };

  void set_rtt_ms(int64_t rtt_ms);
  int64_t rtt_ms() const;

  bool TimeToSendFullNackList(int64_t now) const;

  // Called on a timer, once a second, on the worker_queue_, to update the RTT,
  // check if we need to send RTCP report, send TMMBR updates and fire events.
  void PeriodicUpdate();

  // Returns true if the module is configured to store packets.
  bool StorePackets() const;

  // Used from RtcpSenderMediator to maybe send rtcp.
  void MaybeSendRtcp();

  // Called when `rtcp_sender_` informs of the next RTCP instant. The method may
  // be called on various sequences, and is called under a RTCPSenderLock.
  void ScheduleRtcpSendEvaluation(TimeDelta duration);

  // Helper method combating too early delayed calls from task queues.
  void MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time);

  // Schedules a call to MaybeSendRtcpAtOrAfterTimestamp delayed by `duration`.
  void ScheduleMaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time,
                                               TimeDelta duration);

  Clock* const clock_;
  TaskRunnerBase* const worker_queue_;

  std::unique_ptr<RtpSenderContext> rtp_sender_;
  RTCPSender rtcp_sender_;
  RTCPReceiver rtcp_receiver_;

  uint16_t packet_overhead_;

  // Send side
  int64_t nack_last_time_sent_full_ms_;
  uint16_t nack_last_seq_number_sent_;

  RtcpRttStats* const rtt_stats_;
  base::RepeatingTaskHandle rtt_update_task_;

  // The processed RTT from RtcpRttStats.
  mutable std::mutex mutex_rtt_;
  int64_t rtt_ms_;

  std::shared_ptr<base::PendingTaskFlag> task_safety_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL2_H_
