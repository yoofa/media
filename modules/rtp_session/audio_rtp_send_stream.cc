/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/audio_rtp_send_stream.h"

#include "base/logging.h"

namespace ave::media::rtp_session {

AudioRtpSendStream::AudioRtpSendStream(const Config& config)
    : config_(config),
      timestamp_mapper_(
          config.rtp_clock_rate_hz == 0 ? 1 : config.rtp_clock_rate_hz,
          config.rtp_rtcp == nullptr ? 0 : config.rtp_rtcp->StartTimestamp()) {
  if (config_.rtp_rtcp == nullptr || config_.clock == nullptr ||
      config_.rtp_rtcp->RtpSender() == nullptr || config_.payload_type > 127 ||
      config_.rtp_clock_rate_hz == 0) {
    AVE_LOG(LS_ERROR) << "AudioRtpSendStream requires an RTP sender and clock";
    return;
  }
  rtp_sender_audio_ = std::make_unique<rtp_rtcp::RTPSenderAudio>(
      config_.clock, config_.rtp_rtcp->RtpSender());
  rtp_sender_audio_->RegisterAudioPayload(
      "audio", static_cast<int8_t>(config_.payload_type),
      config_.rtp_clock_rate_hz, 1, 0);
}

AudioRtpSendStream::~AudioRtpSendStream() = default;

void AudioRtpSendStream::OnFrame(const std::shared_ptr<MediaFrame>& frame) {
  if (rtp_sender_audio_ == nullptr || frame == nullptr || frame->size() == 0 ||
      frame->stream_type() != MediaType::AUDIO || !frame->pts().IsFinite()) {
    return;
  }

  const uint32_t rtp_timestamp = timestamp_mapper_.Map(frame->pts());
  rtp_rtcp::RTPSenderAudio::RtpAudioFrame rtp_frame;
  rtp_frame.type = rtp_rtcp::AudioFrameType::kAudioFrameSpeech;
  rtp_frame.payload = std::span<const uint8_t>(frame->data(), frame->size());
  rtp_frame.payload_id = config_.payload_type;
  rtp_frame.rtp_timestamp = rtp_timestamp;
  rtp_frame.capture_time = frame->pts();

  config_.rtp_rtcp->OnSendingRtpFrame(rtp_timestamp, frame->pts().ms(),
                                      config_.payload_type, false);
  if (!rtp_sender_audio_->SendAudio(rtp_frame)) {
    AVE_LOG(LS_WARNING) << "Failed to send encoded audio RTP frame";
  }
}

}  // namespace ave::media::rtp_session
