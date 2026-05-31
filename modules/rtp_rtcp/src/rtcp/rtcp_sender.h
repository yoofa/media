/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <span>
#include "base/clock.h"
#include "base/random.h"
#include "base/units/data_rate.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/foundation/video_bitrate_allocation.h"
#include "media/modules/rtp_rtcp/api/receive_statistics.h"
#include "media/modules/rtp_rtcp/api/rtcp_statistics.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_nack_stats.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/dlrr.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/loss_notification.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/report_block.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/tmmb_item.h"
#include "media/modules/rtp_rtcp/src/transport/transport.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Types are now defined in ave::media::rtp_rtcp (in rtp_rtcp_defines.h)
// VideoBitrateAllocation is in ave::media

class RTCPReceiver;

class RTCPSender final {
 public:
  struct Configuration {
    // TODO: Remove this temporary conversion utility
    // once rtc_rtcp_impl.cc/h are gone.
    static Configuration FromRtpRtcpConfiguration(
        const RtpRtcpInterface::Configuration& config);

    // True for an audio version of the RTP/RTCP module object, false will
    // create a video version.
    bool audio = false;
    // SSRCs for media and retransmission, respectively.
    // FlexFec SSRC is fetched from `flexfec_sender`.
    uint32_t local_media_ssrc = 0;

    // Transport object that will be called when packets are ready to be sent
    // out on the network.
    Transport* outgoing_transport = nullptr;
    // Estimate RTT as non-sender as described in
    // https://tools.ietf.org/html/rfc3611#section-4.4 and #section-4.5
    bool non_sender_rtt_measurement = false;
    // Optional callback which, if specified, is used by RTCPSender to schedule
    // the next time to evaluate if RTCP should be sent.
    std::function<void(TimeDelta)> schedule_next_rtcp_send_evaluation_function;

    std::optional<TimeDelta> rtcp_report_interval;
    ReceiveStatisticsProvider* receive_statistics = nullptr;
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer = nullptr;
  };

  struct FeedbackState {
    FeedbackState();
    FeedbackState(const FeedbackState&);
    FeedbackState(FeedbackState&&);
    ~FeedbackState();

    uint32_t packets_sent;
    size_t media_bytes_sent;
    DataRate send_bitrate;

    uint32_t remote_sr;
    base::NtpTime last_rr;

    std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis;

    // Used when generating TMMBR.
    RTCPReceiver* receiver;
  };

  RTCPSender(base::Clock* clock, Configuration config);

  RTCPSender() = delete;
  RTCPSender(const RTCPSender&) = delete;
  RTCPSender& operator=(const RTCPSender&) = delete;

  ~RTCPSender();

  RtcpMode Status() const;
  void SetRTCPStatus(RtcpMode method);

  bool Sending() const;
  void SetSendingStatus(const FeedbackState& feedback_state, bool enabled);

  void SetNonSenderRttMeasurement(bool enabled);

  void SetTimestampOffset(uint32_t timestamp_offset);

  void SetLastRtpTime(uint32_t rtp_timestamp,
                      std::optional<Timestamp> capture_time,
                      std::optional<int8_t> payload_type);

  void SetRtpClockRate(int8_t payload_type, int rtp_clock_rate_hz);

  uint32_t SSRC() const;
  void SetSsrc(uint32_t ssrc);

  void SetRemoteSSRC(uint32_t ssrc);

  int32_t SetCNAME(std::string_view cName);

  bool TimeToSendRTCPReport(bool send_keyframe_before_rtp = false) const;

  int32_t SendRTCP(const FeedbackState& feedback_state,
                   RTCPPacketType packetType,
                   int32_t nackSize = 0,
                   const uint16_t* nackList = 0);

  int32_t SendLossNotification(const FeedbackState& feedback_state,
                               uint16_t last_decoded_seq_num,
                               uint16_t last_received_seq_num,
                               bool decodability_flag,
                               bool buffering_allowed);

  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs);

  void UnsetRemb();

  bool TMMBR() const;

  void SetMaxRtpPacketSize(size_t max_packet_size);

  void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set);

  void SetCsrcs(const std::vector<uint32_t>& csrcs);

  void SetTargetBitrate(unsigned int target_bitrate);
  void SetVideoBitrateAllocation(const VideoBitrateAllocation& bitrate);
  void SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets);

 private:
  class RtcpContext;
  class PacketSender;

  std::optional<int32_t> ComputeCompoundRTCPPacket(
      const FeedbackState& feedback_state,
      RTCPPacketType packet_type,
      int32_t nack_size,
      const uint16_t* nack_list,
      PacketSender& sender);

  TimeDelta ComputeTimeUntilNextReport(DataRate send_bitrate);

  // Determine which RTCP messages should be sent and setup flags.
  void PrepareReport(const FeedbackState& feedback_state);

  std::vector<rtcp::ReportBlock> CreateReportBlocks(
      const FeedbackState& feedback_state);

  void BuildSR(const RtcpContext& context, PacketSender& sender);
  void BuildRR(const RtcpContext& context, PacketSender& sender);
  void BuildSDES(const RtcpContext& context, PacketSender& sender);
  void BuildPLI(const RtcpContext& context, PacketSender& sender);
  void BuildREMB(const RtcpContext& context, PacketSender& sender);
  void BuildTMMBR(const RtcpContext& context, PacketSender& sender);
  void BuildTMMBN(const RtcpContext& context, PacketSender& sender);
  void BuildAPP(const RtcpContext& context, PacketSender& sender);
  void BuildLossNotification(const RtcpContext& context, PacketSender& sender);
  void BuildExtendedReports(const RtcpContext& context, PacketSender& sender);
  void BuildBYE(const RtcpContext& context, PacketSender& sender);
  void BuildFIR(const RtcpContext& context, PacketSender& sender);
  void BuildNACK(const RtcpContext& context, PacketSender& sender);

  // `duration` being TimeDelta::Zero() means schedule immediately.
  void SetNextRtcpSendEvaluationDuration(TimeDelta duration);

  base::Clock* const clock_;
  const bool audio_;
  mutable std::mutex mutex_rtcp_sender_;
  uint32_t ssrc_;
  base::Random random_;
  RtcpMode method_;

  Transport* const transport_;

  const TimeDelta report_interval_;
  // Set from
  // RTCPSender::Configuration::schedule_next_rtcp_send_evaluation_function.
  const std::function<void(TimeDelta)>
      schedule_next_rtcp_send_evaluation_function_;

  bool sending_;

  std::optional<Timestamp> next_time_to_send_rtcp_;

  uint32_t timestamp_offset_;
  uint32_t last_rtp_timestamp_;
  std::optional<Timestamp> last_frame_capture_time_;
  // SSRC that we receive on our RTP channel
  uint32_t remote_ssrc_;
  std::string cname_;

  ReceiveStatisticsProvider* receive_statistics_;

  // send CSRCs
  std::vector<uint32_t> csrcs_;

  // Full intra request
  uint8_t sequence_number_fir_;

  rtcp::LossNotification loss_notification_;

  // REMB
  int64_t remb_bitrate_;
  std::vector<uint32_t> remb_ssrcs_;

  std::vector<rtcp::TmmbItem> tmmbn_to_send_;
  uint32_t tmmbr_send_bps_;
  uint32_t packet_oh_send_;
  size_t max_packet_size_;

  // True if sending of XR Receiver reference time report is enabled.
  bool xr_send_receiver_reference_time_enabled_;

  RtcpPacketTypeCounterObserver* const packet_type_counter_observer_;
  RtcpPacketTypeCounter packet_type_counter_;

  RtcpNackStats nack_stats_;

  VideoBitrateAllocation video_bitrate_allocation_;
  bool send_video_bitrate_allocation_;

  std::map<int8_t, int> rtp_clock_rates_khz_;
  int8_t last_payload_type_;

  std::optional<VideoBitrateAllocation> CheckAndUpdateLayerStructure(
      const VideoBitrateAllocation& bitrate) const;

  void SetFlag(uint32_t type, bool is_volatile);
  bool IsFlagPresent(uint32_t type) const;
  bool ConsumeFlag(uint32_t type, bool forced = false);
  bool AllVolatileFlagsConsumed() const;

  struct ReportFlag {
    ReportFlag(uint32_t type, bool is_volatile)
        : type(type), is_volatile(is_volatile) {}
    bool operator<(const ReportFlag& flag) const { return type < flag.type; }
    bool operator==(const ReportFlag& flag) const { return type == flag.type; }
    const uint32_t type;
    const bool is_volatile;
  };

  std::set<ReportFlag> report_flags_;

  typedef void (RTCPSender::*BuilderFunc)(const RtcpContext&, PacketSender&);
  // Map from RTCPPacketType to builder.
  std::map<uint32_t, BuilderFunc> builders_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
