/*
 * rtp_sender_audio.h - RTP audio sender
 *
 * Copyright (c) 2024 The WebRTC project authors
 * Copyright (c) 2024 Aspect Software
 *
 * This file is derived from WebRTC's rtp_sender_audio.h
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

#include <span>
#include "base/clock.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sender.h"
#include "media/modules/rtp_rtcp/src/util/absolute_capture_time_sender.h"
#include "media/modules/rtp_rtcp/src/util/dtmf_queue.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Audio frame types (simplified from WebRTC)
enum class AudioFrameType : uint8_t {
  kAudioFrameSpeech = 0,
  kAudioFrameCN = 1,
  kEmptyFrame = 2,
};

class RTPSenderAudio {
 public:
  RTPSenderAudio(base::Clock* clock, RTPSender* rtp_sender);

  RTPSenderAudio() = delete;
  RTPSenderAudio(const RTPSenderAudio&) = delete;
  RTPSenderAudio& operator=(const RTPSenderAudio&) = delete;

  ~RTPSenderAudio();

  int32_t RegisterAudioPayload(std::string_view payload_name,
                               int8_t payload_type,
                               uint32_t frequency,
                               size_t channels,
                               uint32_t rate);

  struct RtpAudioFrame {
    AudioFrameType type = AudioFrameType::kAudioFrameSpeech;
    std::span<const uint8_t> payload;

    // Payload id to write to the payload type field of the rtp packet.
    int32_t payload_id = -1;

    // capture time of the audio frame represented as rtp timestamp.
    uint32_t rtp_timestamp = 0;

    // capture time of the audio frame in the same epoch as `clock->CurrentTime`
    std::optional<Timestamp> capture_time;

    // Audio level in dBov for
    // header-extension-for-audio-level-indication.
    // Valid range is [0,127]. Actual value is negative.
    std::optional<int32_t> audio_level_dbov;

    // Contributing sources list.
    std::span<const uint32_t> csrcs;
  };
  bool SendAudio(const RtpAudioFrame& frame);

  // Send a DTMF tone using RFC 2833 (4733)
  int32_t SendTelephoneEvent(uint8_t key, uint16_t time_ms, uint8_t level);

 protected:
  bool SendTelephoneEventPacket(
      bool ended,
      uint32_t dtmf_timestamp,
      uint16_t duration,
      bool marker_bit);  // set on first packet in talk burst

  bool MarkerBit(AudioFrameType frame_type, int8_t payload_type);

 private:
  base::Clock* const clock_ = nullptr;
  RTPSender* const rtp_sender_ = nullptr;

  std::mutex send_audio_mutex_;

  // DTMF.
  bool dtmf_event_is_on_ = false;
  bool dtmf_event_first_packet_sent_ = false;
  int8_t dtmf_payload_type_ = -1;
  uint32_t dtmf_payload_freq_ = 8000;
  uint32_t dtmf_timestamp_ = 0;
  uint32_t dtmf_length_samples_ = 0;
  int64_t dtmf_time_last_sent_ = 0;
  uint32_t dtmf_timestamp_last_sent_ = 0;
  DtmfQueue::Event dtmf_current_event_;
  DtmfQueue dtmf_queue_;

  // VAD detection, used for marker bit.
  bool inband_vad_active_ = false;
  int8_t cngnb_payload_type_ = -1;
  int8_t cngwb_payload_type_ = -1;
  int8_t cngswb_payload_type_ = -1;
  int8_t cngfb_payload_type_ = -1;
  int8_t last_payload_type_ = -1;

  bool first_packet_sent_ = false;

  std::optional<int32_t> encoder_rtp_timestamp_frequency_;

  AbsoluteCaptureTimeSender absolute_capture_time_sender_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_
