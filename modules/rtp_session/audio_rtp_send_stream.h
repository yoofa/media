/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_SESSION_AUDIO_RTP_SEND_STREAM_H_
#define MEDIA_MODULES_RTP_SESSION_AUDIO_RTP_SEND_STREAM_H_

#include <cstdint>
#include <memory>

#include "base/clock.h"
#include "media/foundation/media_sink_base.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sender_audio.h"
#include "media/modules/rtp_session/rtp_timestamp_mapper.h"

namespace ave::media::rtp_session {

// Packetizes encoded audio MediaFrames and submits them to the RTP sender.
class AudioRtpSendStream final : public MediaFrameSink {
 public:
  struct Config {
    uint8_t payload_type = 96;
    uint32_t rtp_clock_rate_hz = 48000;
    rtp_rtcp::RtpRtcpInterface* rtp_rtcp = nullptr;
    base::Clock* clock = nullptr;
  };

  explicit AudioRtpSendStream(const Config& config);
  ~AudioRtpSendStream() override;

  bool valid() const { return rtp_sender_audio_ != nullptr; }
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override;

 private:
  Config config_;
  RtpTimestampMapper timestamp_mapper_;
  std::unique_ptr<rtp_rtcp::RTPSenderAudio> rtp_sender_audio_;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_AUDIO_RTP_SEND_STREAM_H_
