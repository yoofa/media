/*
 * receive_statistics_impl.h - RTP receive statistics implementation
 * Ported from WebRTC (modules/rtp_rtcp/source/receive_statistics_impl.h)
 *
 * Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the root of the source tree. An additional
 * intellectual property rights grant can be found in the file PATENTS.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "base/bitrate_tracker.h"
#include "base/clock.h"
#include "base/numerics/sequence_number_unwrapper.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/receive_statistics.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/report_block.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace base = ::ave::base;
using Clock = ::ave::base::Clock;
using TimeDelta = ::ave::base::TimeDelta;
using Timestamp = ::ave::base::Timestamp;
using BitrateTracker = ::ave::base::BitrateTracker;
using RtpSequenceNumberUnwrapper = ::ave::base::RtpSequenceNumberUnwrapper;

// Extends StreamStatistician with methods needed by the implementation.
class StreamStatisticianImplInterface : public StreamStatistician {
 public:
  virtual ~StreamStatisticianImplInterface() = default;
  virtual void MaybeAppendReportBlockAndReset(
      std::vector<rtcp::ReportBlock>& report_blocks) = 0;
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) = 0;
  virtual void EnableRetransmitDetection(bool enable) = 0;
  virtual void UpdateCounters(const RtpPacketReceived& packet) = 0;
};

// Thread-compatible implementation of StreamStatisticianImplInterface.
class StreamStatisticianImpl : public StreamStatisticianImplInterface {
 public:
  StreamStatisticianImpl(uint32_t ssrc, Clock* clock);
  ~StreamStatisticianImpl() override;

  // Implements StreamStatistician
  RtpReceiveStats GetStats() const override;
  std::optional<int> GetFractionLostInPercent() const override;
  StreamDataCounters GetReceiveStreamDataCounters() const override;
  uint32_t BitrateReceived() const override;

  // Implements StreamStatisticianImplInterface
  void MaybeAppendReportBlockAndReset(
      std::vector<rtcp::ReportBlock>& report_blocks) override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;
  void EnableRetransmitDetection(bool enable) override;
  // Updates StreamStatistician for incoming packets.
  void UpdateCounters(const RtpPacketReceived& packet) override;

 private:
  bool IsRetransmitOfOldPacket(const RtpPacketReceived& packet,
                               Timestamp now) const;
  void UpdateJitter(const RtpPacketReceived& packet, Timestamp receive_time);
  void ReviseFrequencyAndJitter(int payload_type_frequency);
  // Updates StreamStatistician for out of order packets.
  // Returns true if packet considered to be out of order.
  bool UpdateOutOfOrder(const RtpPacketReceived& packet,
                        int64_t sequence_number,
                        Timestamp now);
  // Checks if this StreamStatistician received any rtp packets.
  bool ReceivedRtpPacket() const { return last_receive_time_.has_value(); }

  const uint32_t ssrc_;
  Clock* const clock_;
  // Delta used to map internal timestamps to Unix epoch ones.
  const TimeDelta delta_internal_unix_epoch_;
  BitrateTracker incoming_bitrate_;
  // In number of packets or sequence numbers.
  int max_reordering_threshold_;
  bool enable_retransmit_detection_;
  bool cumulative_loss_is_capped_;

  // Stats on received RTP packets.
  uint32_t jitter_q4_;
  // Cumulative loss according to RFC 3550, which may be negative (and often is,
  // if packets are reordered and there are non-RTX retransmissions).
  int32_t cumulative_loss_;
  // Offset added to outgoing rtcp reports, to make ensure that the reported
  // cumulative loss is non-negative. Reports with negative values confuse some
  // senders, in particular, our own loss-based bandwidth estimator.
  int32_t cumulative_loss_rtcp_offset_;

  std::optional<Timestamp> last_receive_time_;
  uint32_t last_received_timestamp_;
  RtpSequenceNumberUnwrapper seq_unwrapper_;
  int64_t received_seq_first_;
  int64_t received_seq_max_;
  // Assume that the other side restarted when there are two sequential packets
  // with large jump from received_seq_max_.
  std::optional<uint16_t> received_seq_out_of_order_;

  // Current counter values.
  StreamDataCounters receive_counters_;

  // Counter values when we sent the last report.
  int32_t last_report_cumulative_loss_;
  int64_t last_report_seq_max_;

  // The sample frequency of the last received packet.
  int last_payload_type_frequency_;
};

// Thread-safe implementation of StreamStatisticianImplInterface.
class StreamStatisticianLocked : public StreamStatisticianImplInterface {
 public:
  StreamStatisticianLocked(uint32_t ssrc, Clock* clock) : impl_(ssrc, clock) {}
  ~StreamStatisticianLocked() override = default;

  RtpReceiveStats GetStats() const override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    return impl_.GetStats();
  }
  std::optional<int> GetFractionLostInPercent() const override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    return impl_.GetFractionLostInPercent();
  }
  StreamDataCounters GetReceiveStreamDataCounters() const override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    return impl_.GetReceiveStreamDataCounters();
  }
  uint32_t BitrateReceived() const override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    return impl_.BitrateReceived();
  }
  void MaybeAppendReportBlockAndReset(
      std::vector<rtcp::ReportBlock>& report_blocks) override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    impl_.MaybeAppendReportBlockAndReset(report_blocks);
  }
  void SetMaxReorderingThreshold(int max_reordering_threshold) override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    return impl_.SetMaxReorderingThreshold(max_reordering_threshold);
  }
  void EnableRetransmitDetection(bool enable) override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    return impl_.EnableRetransmitDetection(enable);
  }
  void UpdateCounters(const RtpPacketReceived& packet) override {
    std::lock_guard<std::mutex> lock(stream_lock_);
    return impl_.UpdateCounters(packet);
  }

 private:
  mutable std::mutex stream_lock_;
  StreamStatisticianImpl impl_;
};

// Thread-compatible implementation.
class ReceiveStatisticsImpl : public ReceiveStatistics {
 public:
  ReceiveStatisticsImpl(
      Clock* clock,
      std::function<std::unique_ptr<StreamStatisticianImplInterface>(
          uint32_t ssrc,
          Clock* clock)> stream_statistician_factory);
  ~ReceiveStatisticsImpl() override = default;

  // Implements ReceiveStatisticsProvider.
  std::vector<rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks) override;

  // Implements RtpPacketSinkInterface
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  // Implements ReceiveStatistics.
  StreamStatistician* GetStatistician(uint32_t ssrc) const override;
  void SetMaxReorderingThreshold(uint32_t ssrc,
                                 int max_reordering_threshold) override;
  void EnableRetransmitDetection(uint32_t ssrc, bool enable) override;

 private:
  StreamStatisticianImplInterface* GetOrCreateStatistician(uint32_t ssrc);

  Clock* const clock_;
  std::function<std::unique_ptr<StreamStatisticianImplInterface>(uint32_t ssrc,
                                                                 Clock* clock)>
      stream_statistician_factory_;
  // The index within `all_ssrcs_` that was last returned.
  size_t last_returned_ssrc_idx_;
  std::vector<uint32_t> all_ssrcs_;
  std::map<uint32_t /*ssrc*/, std::unique_ptr<StreamStatisticianImplInterface>>
      statisticians_;
};

// Thread-safe implementation wrapping access to ReceiveStatisticsImpl with a
// mutex.
class ReceiveStatisticsLocked : public ReceiveStatistics {
 public:
  explicit ReceiveStatisticsLocked(
      Clock* clock,
      std::function<std::unique_ptr<StreamStatisticianImplInterface>(
          uint32_t ssrc,
          Clock* clock)> stream_statitician_factory)
      : impl_(clock, std::move(stream_statitician_factory)) {}
  ~ReceiveStatisticsLocked() override = default;
  std::vector<rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks) override {
    std::lock_guard<std::mutex> lock(receive_statistics_lock_);
    return impl_.RtcpReportBlocks(max_blocks);
  }
  void OnRtpPacket(const RtpPacketReceived& packet) override {
    std::lock_guard<std::mutex> lock(receive_statistics_lock_);
    return impl_.OnRtpPacket(packet);
  }
  StreamStatistician* GetStatistician(uint32_t ssrc) const override {
    std::lock_guard<std::mutex> lock(receive_statistics_lock_);
    return impl_.GetStatistician(ssrc);
  }
  void SetMaxReorderingThreshold(uint32_t ssrc,
                                 int max_reordering_threshold) override {
    std::lock_guard<std::mutex> lock(receive_statistics_lock_);
    return impl_.SetMaxReorderingThreshold(ssrc, max_reordering_threshold);
  }
  void EnableRetransmitDetection(uint32_t ssrc, bool enable) override {
    std::lock_guard<std::mutex> lock(receive_statistics_lock_);
    return impl_.EnableRetransmitDetection(ssrc, enable);
  }

 private:
  mutable std::mutex receive_statistics_lock_;
  ReceiveStatisticsImpl impl_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
