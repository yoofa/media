/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_
#define AVE_MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "media/modules/rtp_rtcp/api/rtcp_statistics.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sink_interface.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/report_block.h"

namespace ave {
namespace base {
class Clock;
}  // namespace base

namespace media {
namespace rtp_rtcp {

using rtcp::ReportBlock;

class ReceiveStatisticsProvider {
 public:
  virtual ~ReceiveStatisticsProvider() = default;
  // Collects receive statistic in a form of rtcp report blocks.
  // Returns at most `max_blocks` report blocks.
  virtual std::vector<ReportBlock> RtcpReportBlocks(size_t max_blocks) = 0;
};

class StreamStatistician {
 public:
  virtual ~StreamStatistician();

  virtual RtpReceiveStats GetStats() const = 0;

  // Returns average over the stream life time.
  virtual std::optional<int32_t> GetFractionLostInPercent() const = 0;

  // TODO(bugs.webrtc.org/10679): Delete, migrate users to the above GetStats
  // method (and extend RtpReceiveStats if needed).
  // Gets receive stream data counters.
  virtual StreamDataCounters GetReceiveStreamDataCounters() const = 0;

  virtual uint32_t BitrateReceived() const = 0;
};

class ReceiveStatistics : public ReceiveStatisticsProvider,
                          public RtpPacketSinkInterface {
 public:
  ~ReceiveStatistics() override = default;

  // Returns a thread-safe instance of ReceiveStatistics.
  static std::unique_ptr<ReceiveStatistics> Create(base::Clock* clock);
  // Returns a thread-compatible instance of ReceiveStatistics.
  static std::unique_ptr<ReceiveStatistics> CreateThreadCompatible(
      base::Clock* clock);

  // Returns a pointer to the statistician of an ssrc.
  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const = 0;

  // Sets the max reordering threshold in number of packets.
  virtual void SetMaxReorderingThreshold(uint32_t ssrc,
                                         int32_t max_reordering_threshold) = 0;
  // Detect retransmissions, enabling updates of the retransmitted counters. The
  // default is false.
  virtual void EnableRetransmitDetection(uint32_t ssrc, bool enable) = 0;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_
