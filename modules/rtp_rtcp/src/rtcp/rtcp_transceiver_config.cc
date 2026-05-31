/*
 * rtcp_transceiver_config.cc - RTCP transceiver configuration implementation
 *
 * Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-oriented video encoder by aspect team.
 */

#include "media/modules/rtp_rtcp/src/rtcp/rtcp_transceiver_config.h"

#include "base/logging.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

RtcpTransceiverConfig::RtcpTransceiverConfig() = default;
RtcpTransceiverConfig::RtcpTransceiverConfig(const RtcpTransceiverConfig&) =
    default;
RtcpTransceiverConfig& RtcpTransceiverConfig::operator=(
    const RtcpTransceiverConfig&) = default;
RtcpTransceiverConfig::~RtcpTransceiverConfig() = default;

bool RtcpTransceiverConfig::Validate() const {
  if (feedback_ssrc == 0) {
    AVE_LOG(LS_WARNING)
        << debug_id
        << "Ssrc 0 may be treated by some implementation as invalid.";
  }
  if (cname.size() > 255) {
    AVE_LOG(LS_ERROR) << debug_id << "cname can be maximum 255 characters.";
    return false;
  }
  if (max_packet_size < 100) {
    AVE_LOG(LS_ERROR) << debug_id << "max packet size " << max_packet_size
                      << " is too small.";
    return false;
  }
  if (max_packet_size > IP_PACKET_SIZE) {
    AVE_LOG(LS_ERROR) << debug_id << "max packet size " << max_packet_size
                      << " more than " << IP_PACKET_SIZE << " is unsupported.";
    return false;
  }
  if (clock == nullptr) {
    AVE_LOG(LS_ERROR) << debug_id << "clock must be set";
    return false;
  }
  if (initial_report_delay < TimeDelta::Zero()) {
    AVE_LOG(LS_ERROR) << debug_id << "delay " << initial_report_delay.ms()
                      << "ms before first report shouldn't be negative.";
    return false;
  }
  if (report_period <= TimeDelta::Zero()) {
    AVE_LOG(LS_ERROR) << debug_id << "period " << report_period.ms()
                      << "ms between reports should be positive.";
    return false;
  }
  if (rtcp_mode != RtcpMode::kCompound && rtcp_mode != RtcpMode::kReducedSize) {
    AVE_LOG(LS_ERROR) << debug_id << "unsupported rtcp mode";
    return false;
  }
  if (non_sender_rtt_measurement && !network_link_observer) {
    AVE_LOG(LS_WARNING) << debug_id
                        << "Enabled special feature to calculate rtt, but no "
                           "rtt observer is provided.";
  }
  return true;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
