/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_SESSION_VIDEO_RTP_SEND_STREAM_H_
#define MEDIA_MODULES_RTP_SESSION_VIDEO_RTP_SEND_STREAM_H_

#include <cstdint>
#include <memory>

#include "base/clock.h"
#include "media/foundation/h264/h264_globals.h"
#include "media/foundation/media_sink_base.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sender_video.h"
#include "media/modules/rtp_session/rtp_timestamp_mapper.h"

namespace ave::media::rtp_session {

// Packetizes complete Annex-B H.264 access units into RFC 6184 RTP packets.
class VideoRtpSendStream final : public MediaFrameSink {
 public:
  struct Config {
    uint8_t payload_type = 102;
    uint32_t rtp_clock_rate_hz = 90000;
    H264PacketizationMode packetization_mode =
        H264PacketizationMode::NonInterleaved;
    rtp_rtcp::RtpRtcpInterface* rtp_rtcp = nullptr;
    base::Clock* clock = nullptr;
  };

  explicit VideoRtpSendStream(const Config& config);
  ~VideoRtpSendStream() override;

  bool valid() const { return rtp_sender_video_ != nullptr; }
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override;

 private:
  Config config_;
  RtpTimestampMapper timestamp_mapper_;
  std::unique_ptr<rtp_rtcp::RTPSenderVideo> rtp_sender_video_;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_VIDEO_RTP_SEND_STREAM_H_
