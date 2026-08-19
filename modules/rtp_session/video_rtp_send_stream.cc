/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_session/video_rtp_send_stream.h"

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/units/time_delta.h"
#include "media/foundation/h264/avc_utils.h"
#include "media/foundation/video_codec_type.h"
#include "media/foundation/video_frame_type.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"

namespace ave::media::rtp_session {
namespace {

std::optional<uint16_t> ToRtpDimension(int32_t dimension) {
  if (!std::in_range<uint16_t>(dimension)) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(dimension);
}

}  // namespace

VideoRtpSendStream::VideoRtpSendStream(const Config& config)
    : config_(config),
      timestamp_mapper_(
          config.rtp_clock_rate_hz,
          config.rtp_rtcp == nullptr ? 0 : config.rtp_rtcp->StartTimestamp()) {
  if (config_.rtp_rtcp == nullptr || config_.clock == nullptr ||
      config_.rtp_rtcp->RtpSender() == nullptr || config_.payload_type > 127 ||
      config_.rtp_clock_rate_hz == 0) {
    AVE_LOG(LS_ERROR) << "VideoRtpSendStream requires an RTP sender and clock";
    return;
  }

  config_.rtp_rtcp->RegisterSendPayloadFrequency(
      config_.payload_type, static_cast<int32_t>(config_.rtp_clock_rate_hz));
  rtp_rtcp::RTPSenderVideo::Config sender_config;
  sender_config.clock = config_.clock;
  sender_config.rtp_sender = config_.rtp_rtcp->RtpSender();
  sender_config.enable_retransmit_all_layers = true;
  rtp_sender_video_ =
      std::make_unique<rtp_rtcp::RTPSenderVideo>(std::move(sender_config));
}

VideoRtpSendStream::~VideoRtpSendStream() = default;

void VideoRtpSendStream::OnFrame(const std::shared_ptr<MediaFrame>& frame) {
  if (rtp_sender_video_ == nullptr || frame == nullptr || frame->size() == 0 ||
      frame->stream_type() != MediaType::VIDEO ||
      frame->codec() != CodecId::AVE_CODEC_ID_H264 ||
      !frame->pts().IsFinite()) {
    return;
  }
  const std::optional<uint16_t> width = ToRtpDimension(frame->width());
  const std::optional<uint16_t> height = ToRtpDimension(frame->height());
  if (!width.has_value() || !height.has_value()) {
    AVE_LOG(LS_WARNING) << "Dropping H.264 frame with invalid dimensions";
    return;
  }

  const bool is_key_frame = frame->picture_type() == PictureType::I ||
                            IsIDR(frame->data(), frame->size());
  rtp_rtcp::RTPVideoHeader video_header;
  video_header.codec = VideoCodecType::kVideoCodecH264;
  video_header.frame_type = is_key_frame ? VideoFrameType::kVideoFrameKey
                                         : VideoFrameType::kVideoFrameDelta;
  video_header.width = *width;
  video_header.height = *height;
  video_header.is_first_packet_in_frame = true;
  video_header.is_last_packet_in_frame = true;
  RTPVideoHeaderH264 h264_header{};
  h264_header.packetization_mode = config_.packetization_mode;
  video_header.video_type_header = std::move(h264_header);

  const uint32_t rtp_timestamp = timestamp_mapper_.Map(frame->pts());
  const base::Timestamp capture_time = frame->arrival_time().IsFinite()
                                           ? frame->arrival_time()
                                           : config_.clock->CurrentTime();
  config_.rtp_rtcp->OnSendingRtpFrame(rtp_timestamp, capture_time.ms(),
                                      config_.payload_type, is_key_frame);
  const bool sent = rtp_sender_video_->SendVideo(
      config_.payload_type, VideoCodecType::kVideoCodecH264, rtp_timestamp,
      capture_time, std::span<const uint8_t>(frame->data(), frame->size()),
      frame->size(), std::move(video_header),
      config_.rtp_rtcp->ExpectedRetransmissionTime(), {});
  if (!sent) {
    AVE_LOG(LS_WARNING) << "Failed to send encoded H.264 RTP frame";
  }
}

}  // namespace ave::media::rtp_session
