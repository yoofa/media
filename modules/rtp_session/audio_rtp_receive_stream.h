/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_SESSION_AUDIO_RTP_RECEIVE_STREAM_H_
#define MEDIA_MODULES_RTP_SESSION_AUDIO_RTP_RECEIVE_STREAM_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>

#include "base/task_util/task_runner_factory.h"
#include "media/codec/codec_id.h"
#include "media/foundation/async_media_frame_source.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sink_interface.h"
#include "media/modules/rtp_session/audio_packet_buffer.h"
#include "media/modules/rtp_session/rtp_timestamp_mapper.h"

namespace ave::media::rtp_session {

class AudioRtpReceiveStream final : public AsyncMediaFrameSource,
                                    public rtp_rtcp::RtpPacketSinkInterface {
 public:
  struct Config {
    uint32_t remote_ssrc = 0;
    uint32_t rtp_clock_rate_hz = 48000;
    uint32_t default_frame_duration_ms = 20;
    std::map<uint8_t, CodecId> payload_type_codecs;
    AudioPacketBuffer::Config packet_buffer;
    base::TaskRunnerFactory* task_runner_factory = nullptr;
  };

  explicit AudioRtpReceiveStream(const Config& config);
  ~AudioRtpReceiveStream() override;

  void Start();
  void Stop();
  void OnRtpPacket(const rtp_rtcp::RtpPacketReceived& packet) override;
  AudioPacketBufferStats GetPacketBufferStats() const;

 private:
  void DeliverPacket(BufferedAudioPacket packet);

  Config config_;
  AudioPacketBuffer packet_buffer_;
  RtpReceiveTimestampMapper timestamp_mapper_;
  mutable std::mutex mutex_;
  std::atomic<bool> running_{false};
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_AUDIO_RTP_RECEIVE_STREAM_H_
