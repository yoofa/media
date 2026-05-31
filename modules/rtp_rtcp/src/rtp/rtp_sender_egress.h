/*
 * rtp_sender_egress.h
 * Copyright (C) 2024 yoofa <vsyfar@gmail.com>
 *
 * Ported from WebRTC project authors.
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_EGRESS_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_EGRESS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <span>
#include "base/bitrate_tracker.h"
#include "base/clock.h"
#include "base/task_util/pending_task_flag.h"
#include "base/task_util/repeating_task.h"
#include "base/task_util/task_runner_base.h"
#include "base/units/timestamp.h"
#include "media/modules/include/module_fec_types.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/fec/video_fec_generator.h"
#include "media/modules/rtp_rtcp/src/rtp/packet_sequencer.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_history.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sequence_number_map.h"
#include "media/modules/rtp_rtcp/src/transport/network_types.h"
#include "media/modules/rtp_rtcp/src/transport/transport.h"

namespace ave {

using BitrateTracker = ::ave::base::BitrateTracker;

namespace media {
namespace rtp_rtcp {

class RtpSenderEgress {
 public:
  // Helper class that redirects packets directly to the send part of this class
  // without passing through an actual paced sender.
  class NonPacedPacketSender : public RtpPacketSender {
   public:
    NonPacedPacketSender(base::TaskRunnerBase* worker_queue,
                         RtpSenderEgress* sender,
                         PacketSequencer* sequencer);
    virtual ~NonPacedPacketSender();

    void EnqueuePackets(
        std::vector<std::unique_ptr<RtpPacketToSend>> packets) override;
    // Since we don't pace packets, there's no pending packets to remove.
    void RemovePacketsForSsrc(uint32_t /* ssrc */) override {}

   private:
    void PrepareForSend(RtpPacketToSend* packet);
    base::TaskRunnerBase* worker_queue_;
    uint16_t transport_sequence_number_;
    RtpSenderEgress* const sender_;
    PacketSequencer* sequencer_;
    std::shared_ptr<base::PendingTaskFlag> task_safety_;
  };

  RtpSenderEgress(base::Clock* clock,
                  const RtpRtcpInterface::Configuration& config,
                  RtpPacketHistory* packet_history);
  ~RtpSenderEgress();

  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& pacing_info);
  void OnBatchComplete();
  uint32_t Ssrc() const { return ssrc_; }
  std::optional<uint32_t> RtxSsrc() const { return rtx_ssrc_; }
  std::optional<uint32_t> FlexFecSsrc() const { return flexfec_ssrc_; }

  RtpSendRates GetSendRates(base::Timestamp now) const;
  void GetDataCounters(StreamDataCounters* rtp_stats,
                       StreamDataCounters* rtx_stats) const;

  void ForceIncludeSendPacketsInAllocation(bool part_of_allocation);

  bool MediaHasBeenSent() const;
  void SetMediaHasBeenSent(bool media_sent);
  void SetTimestampOffset(uint32_t timestamp);

  // For each sequence number in `sequence_number`, recall the last RTP packet
  // which bore it - its timestamp and whether it was the first and/or last
  // packet in that frame. If all of the given sequence numbers could be
  // recalled, return a vector with all of them (in corresponding order).
  // If any could not be recalled, return an empty vector.
  std::vector<RtpSequenceNumberMap::Info> GetSentRtpPacketInfos(
      std::span<const uint16_t> sequence_numbers) const;

  void SetFecProtectionParameters(const FecProtectionParams& delta_params,
                                  const FecProtectionParams& key_params);
  std::vector<std::unique_ptr<RtpPacketToSend>> FetchFecPackets();

  // Clears pending status for these sequence numbers in the packet history.
  void OnAbortedRetransmissions(std::span<const uint16_t> sequence_numbers);

 private:
  struct Packet {
    std::unique_ptr<RtpPacketToSend> rtp_packet;
    PacedPacketInfo info;
    base::Timestamp now;
  };
  void CompleteSendPacket(const Packet& compound_packet, bool last_in_batch);
  bool HasCorrectSsrc(const RtpPacketToSend& packet) const;

  // Sends packet on to `transport_`, leaving the RTP module.
  bool SendPacketToNetwork(const RtpPacketToSend& packet,
                           const PacketOptions& options,
                           const PacedPacketInfo& pacing_info);
  void UpdateRtpStats(base::Timestamp now,
                      uint32_t packet_ssrc,
                      RtpPacketMediaType packet_type,
                      RtpPacketCounter counter,
                      size_t packet_size);

  // Called on a timer, once a second, on the worker_queue_.
  void PeriodicUpdate();

  base::Clock* const clock_;
  const bool enable_send_packet_batching_;
  base::TaskRunnerBase* const worker_queue_;
  const uint32_t ssrc_;
  const std::optional<uint32_t> rtx_ssrc_;
  const std::optional<uint32_t> flexfec_ssrc_;
  // TODO: These are used for VideoTimingExtension which is not yet ported
  // const bool populate_network2_timestamp_;
  // const bool use_ntp_time_for_absolute_send_time_;
  RtpPacketHistory* const packet_history_;
  Transport* const transport_;
  const bool is_audio_;
  const bool need_rtp_packet_infos_;
  VideoFecGenerator* const fec_generator_;
  std::optional<uint16_t> last_sent_seq_;
  std::optional<uint16_t> last_sent_rtx_seq_;

  SendPacketObserver* const send_packet_observer_;
  StreamDataCountersCallback* const rtp_stats_callback_;
  BitrateStatisticsObserver* const bitrate_callback_;

  bool media_has_been_sent_;
  bool force_part_of_allocation_;
  uint32_t timestamp_offset_;

  StreamDataCounters rtp_stats_;
  StreamDataCounters rtx_rtp_stats_;
  // One element per value in RtpPacketMediaType, with index matching value.
  std::vector<BitrateTracker> send_rates_;
  std::optional<std::pair<FecProtectionParams, FecProtectionParams>>
      pending_fec_params_;

  // Maps sent packets' sequence numbers to a tuple consisting of:
  // 1. The timestamp, without the randomizing offset mandated by the RFC.
  // 2. Whether the packet was the first in its frame.
  // 3. Whether the packet was the last in its frame.
  const std::unique_ptr<RtpSequenceNumberMap> rtp_sequence_number_map_;
  base::RepeatingTaskHandle update_task_;
  std::vector<Packet> packets_to_send_;
  std::shared_ptr<base::PendingTaskFlag> task_safety_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_EGRESS_H_
