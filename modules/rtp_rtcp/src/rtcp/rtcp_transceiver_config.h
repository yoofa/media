/*
 * rtcp_transceiver_config.h - RTCP transceiver configuration
 *
 * Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-oriented video encoder by aspect team.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_

#include <functional>
#include <string>

#include <span>
#include "base/clock.h"
#include "base/task_util/task_runner_base.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/foundation/video_bitrate_allocation.h"
#include "media/modules/rtp_rtcp/api/report_block_data.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/util/ntp_time.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Forward declaration
class ReceiveStatisticsProvider;

// Interface to watch incoming rtcp packets by media (rtp) receiver.
// All message handlers have default empty implementation. This way users only
// need to implement the ones they are interested in.
class MediaReceiverRtcpObserver {
 public:
  virtual ~MediaReceiverRtcpObserver() = default;

  virtual void OnSenderReport(uint32_t /* sender_ssrc */,
                              NtpTime /* ntp_time */,
                              uint32_t /* rtp_time */) {}
  virtual void OnBye(uint32_t /* sender_ssrc */) {}
  virtual void OnBitrateAllocation(
      uint32_t /* sender_ssrc */,
      const VideoBitrateAllocation& /* allocation */) {}
};

// Handles RTCP related messages for a single RTP stream (i.e. single SSRC)
class RtpStreamRtcpHandler {
 public:
  virtual ~RtpStreamRtcpHandler() = default;

  // Statistic about sent RTP packets to propagate to RTCP sender report.
  class RtpStats {
   public:
    RtpStats() = default;
    RtpStats(const RtpStats&) = default;
    RtpStats& operator=(const RtpStats&) = default;
    ~RtpStats() = default;

    size_t num_sent_packets() const { return num_sent_packets_; }
    size_t num_sent_bytes() const { return num_sent_bytes_; }
    Timestamp last_capture_time() const { return last_capture_time_; }
    uint32_t last_rtp_timestamp() const { return last_rtp_timestamp_; }
    int32_t last_clock_rate() const { return last_clock_rate_; }

    void set_num_sent_packets(size_t v) { num_sent_packets_ = v; }
    void set_num_sent_bytes(size_t v) { num_sent_bytes_ = v; }
    void set_last_capture_time(Timestamp v) { last_capture_time_ = v; }
    void set_last_rtp_timestamp(uint32_t v) { last_rtp_timestamp_ = v; }
    void set_last_clock_rate(int32_t v) { last_clock_rate_ = v; }

   private:
    size_t num_sent_packets_ = 0;
    size_t num_sent_bytes_ = 0;
    Timestamp last_capture_time_ = Timestamp::Zero();
    uint32_t last_rtp_timestamp_ = 0;
    int32_t last_clock_rate_ = 90'000;
  };
  virtual RtpStats SentStats() = 0;

  virtual void OnNack(uint32_t /* sender_ssrc */,
                      std::span<const uint16_t> /* sequence_numbers */) {}
  virtual void OnFir(uint32_t /* sender_ssrc */) {}
  virtual void OnPli(uint32_t /* sender_ssrc */) {}

  // Called on an RTCP packet with sender or receiver reports with a report
  // block for the handled RTP stream.
  virtual void OnReport(const ReportBlockData& /* report_block */) {}
};

struct RtcpTransceiverConfig {
  RtcpTransceiverConfig();
  RtcpTransceiverConfig(const RtcpTransceiverConfig&);
  RtcpTransceiverConfig& operator=(const RtcpTransceiverConfig&);
  ~RtcpTransceiverConfig();

  // Logs the error and returns false if configuration miss key objects or
  // is inconsistant. May log warnings.
  bool Validate() const;

  // Used to prepend all log messages. Can be empty.
  std::string debug_id;

  // Ssrc to use as default sender ssrc, e.g. for transport-wide feedbacks.
  uint32_t feedback_ssrc = 1;

  // Canonical End-Point Identifier of the local particiapnt.
  // Defined in rfc3550 section 6 note 2 and section 6.5.1.
  std::string cname;

  // Maximum packet size outgoing transport accepts.
  size_t max_packet_size = 1200;

  // The clock to use when querying for the NTP time. Should be set.
  base::Clock* clock = nullptr;

  // Task runner for scheduling periodic tasks.
  // If nullptr, schedule_periodic_compound_packets must be false.
  base::TaskRunnerBase* task_runner = nullptr;

  // Transport to send RTCP packets to.
  std::function<void(std::span<const uint8_t>)> rtcp_transport;

  // Rtcp report block generator for outgoing receiver reports.
  ReceiveStatisticsProvider* receive_statistics = nullptr;

  // Should outlive RtcpTransceiver.
  // Callbacks will be invoked on the caller's thread.
  NetworkLinkRtcpObserver* network_link_observer = nullptr;

  // Configures if sending should
  //  enforce compound packets: https://tools.ietf.org/html/rfc4585#section-3.1
  //  or allow reduced size packets: https://tools.ietf.org/html/rfc5506
  // Receiving accepts both compound and reduced-size packets.
  RtcpMode rtcp_mode = RtcpMode::kCompound;

  //
  // Tuning parameters.
  //
  // Initial flag if `rtcp_transport` can be used to send packets.
  // If set to false, RtcpTransciever won't call `rtcp_transport` until
  // SetReadyToSend(true) is called.
  bool initial_ready_to_send = true;

  // Delay before 1st periodic compound packet.
  TimeDelta initial_report_delay = TimeDelta::Millis(500);

  // Period between periodic compound packets.
  TimeDelta report_period = TimeDelta::Seconds(1);

  //
  // Flags for features and experiments.
  //
  bool schedule_periodic_compound_packets =
      false;  // Disabled without TaskQueue
  // Estimate RTT as non-sender as described in
  // https://tools.ietf.org/html/rfc3611#section-4.4 and #section-4.5
  bool non_sender_rtt_measurement = false;

  // Reply to incoming RRTR messages so that remote endpoint may estimate RTT as
  // non-sender as described in https://tools.ietf.org/html/rfc3611#section-4.4
  // and #section-4.5
  bool reply_to_non_sender_rtt_measurement = true;

  // Reply to incoming RRTR messages multiple times, one per sender SSRC, to
  // support clients that calculate and process RTT per sender SSRC.
  bool reply_to_non_sender_rtt_mesaurments_on_all_ssrcs = true;

  // Allows a REMB message to be sent immediately when SetRemb is called without
  // having to wait for the next compount message to be sent.
  bool send_remb_on_change = false;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_
