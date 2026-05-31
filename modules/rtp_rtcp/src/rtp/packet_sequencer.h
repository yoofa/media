/*
 * packet_sequencer.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_PACKET_SEQUENCER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_PACKET_SEQUENCER_H_

#include <optional>

#include "base/clock.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Helper class used to assign RTP sequence numbers and populate some fields for
// padding packets based on the last sequenced packets.
// This class is not thread safe, the caller must provide that.
class PacketSequencer {
 public:
  // If `require_marker_before_media_padding_` is true, padding packets on the
  // media ssrc is not allowed unless the last sequenced media packet had the
  // marker bit set (i.e. don't insert padding packets between the first and
  // last packets of a video frame).
  // Packets with unknown SSRCs will be ignored.
  PacketSequencer(uint32_t media_ssrc,
                  std::optional<uint32_t> rtx_ssrc,
                  bool require_marker_before_media_padding,
                  base::Clock* clock);

  // Assigns sequence number, and in the case of non-RTX padding also timestamps
  // and payload type.
  void Sequence(RtpPacketToSend& packet);

  void set_media_sequence_number(uint16_t sequence_number) {
    media_sequence_number_ = sequence_number;
  }
  void set_rtx_sequence_number(uint16_t sequence_number) {
    rtx_sequence_number_ = sequence_number;
  }

  void SetRtpState(const RtpState& state);
  void PopulateRtpState(RtpState& state) const;

  uint16_t media_sequence_number() const { return media_sequence_number_; }
  uint16_t rtx_sequence_number() const { return rtx_sequence_number_; }

  // Checks whether it is allowed to send padding on the media SSRC at this
  // time, e.g. that we don't send padding in the middle of a video frame.
  bool CanSendPaddingOnMediaSsrc() const;

 private:
  void UpdateLastPacketState(const RtpPacketToSend& packet);
  void PopulatePaddingFields(RtpPacketToSend& packet);

  const uint32_t media_ssrc_;
  const std::optional<uint32_t> rtx_ssrc_;
  const bool require_marker_before_media_padding_;
  base::Clock* const clock_;

  uint16_t media_sequence_number_;
  uint16_t rtx_sequence_number_;

  int8_t last_payload_type_;
  uint32_t last_rtp_timestamp_;
  base::Timestamp last_capture_time_ = base::Timestamp::MinusInfinity();
  base::Timestamp last_timestamp_time_ = base::Timestamp::MinusInfinity();
  bool last_packet_marker_bit_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_PACKET_SEQUENCER_H_
