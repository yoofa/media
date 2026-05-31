/*
 * rtp_sender.h - RTP Sender
 *
 * Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 * Copyright (c) 2024 The aspect project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source tree.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <span>
#include "base/clock.h"
#include "base/constructor_magic.h"
#include "base/random.h"
#include "base/types.h"
#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sender.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extension_size.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_history.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RateLimiter;

// Maximum amount of padding in RFC 3550 is 255 bytes.
constexpr size_t kMaxPaddingLength = 255;

class RTPSender {
 public:
  RTPSender(base::Clock* clock,
            const RtpRtcpInterface::Configuration& config,
            RtpPacketHistory* packet_history,
            RtpPacketSender* packet_sender);

  ~RTPSender();

  void SetSendingMediaStatus(bool enabled);
  bool SendingMedia() const;
  bool IsAudioConfigured() const;

  uint32_t TimestampOffset() const;
  void SetTimestampOffset(uint32_t timestamp);

  void SetMid(std::string_view mid);

  uint16_t SequenceNumber() const;
  void SetSequenceNumber(uint16_t seq);

  void SetMaxRtpPacketSize(size_t max_packet_size);

  void SetExtmapAllowMixed(bool extmap_allow_mixed);

  // RTP header extension
  bool RegisterRtpHeaderExtension(std::string_view uri, int id);
  bool IsRtpHeaderExtensionRegistered(RTPExtensionType type) const;
  void DeregisterRtpHeaderExtension(std::string_view uri);

  bool SupportsPadding() const;
  bool SupportsRtxPayloadPadding() const;

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes,
      bool media_has_been_sent,
      bool can_send_padding_on_media_ssrc);

  // NACK.
  void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers,
                      int64_t avg_rtt);

  int32_t ReSendPacket(uint16_t packet_id);

  // ACK.
  void OnReceivedAckOnSsrc(int64_t extended_highest_sequence_number);
  void OnReceivedAckOnRtxSsrc(int64_t extended_highest_sequence_number);

  // RTX.
  void SetRtxStatus(int mode);
  int RtxStatus() const;
  std::optional<uint32_t> RtxSsrc() const { return rtx_ssrc_; }
  // Returns expected size difference between an RTX packet and media packet
  // that RTX packet is created from. Returns 0 if RTX is disabled.
  size_t RtxPacketOverhead() const;

  void SetRtxPayloadType(int payload_type, int associated_payload_type);

  // Size info for header extensions used by FEC packets.
  static std::span<const RtpExtensionSize> FecExtensionSizes();

  // Size info for header extensions used by video packets.
  static std::span<const RtpExtensionSize> VideoExtensionSizes();

  // Size info for header extensions used by audio packets.
  static std::span<const RtpExtensionSize> AudioExtensionSizes();

  // Create empty packet, fills ssrc, csrcs and reserve place for header
  // extensions RtpSender updates before sending.
  std::unique_ptr<RtpPacketToSend> AllocatePacket(
      std::span<const uint32_t> csrcs = {});

  // Maximum header overhead per fec/padding packet.
  size_t FecOrPaddingPacketMaxRtpHeaderLength() const;
  // Expected header overhead per media packet.
  size_t ExpectedPerPacketOverhead() const;
  // Including RTP headers.
  size_t MaxRtpPacketSize() const;

  uint32_t SSRC() const { return ssrc_; }

  std::optional<uint32_t> FlexfecSsrc() const { return flexfec_ssrc_; }

  // Pass a set of packets to RtpPacketSender instance, for paced or immediate
  // sending to the network.
  void EnqueuePackets(std::vector<std::unique_ptr<RtpPacketToSend>> packets);

  void SetRtpState(const RtpState& rtp_state);
  RtpState GetRtpState() const;
  void SetRtxRtpState(const RtpState& rtp_state);
  RtpState GetRtxRtpState() const;

 private:
  std::unique_ptr<RtpPacketToSend> BuildRtxPacket(
      const RtpPacketToSend& packet);

  bool IsFecPacket(const RtpPacketToSend& packet) const;

  void UpdateHeaderSizes();

  void UpdateLastPacketState(const RtpPacketToSend& packet);

  base::Clock* const clock_;
  base::Random random_;

  const bool audio_configured_;

  const uint32_t ssrc_;
  const std::optional<uint32_t> rtx_ssrc_;
  const std::optional<uint32_t> flexfec_ssrc_;

  RtpPacketHistory* const packet_history_;
  RtpPacketSender* const paced_sender_;

  mutable std::mutex send_mutex_;

  bool sending_media_;
  size_t max_packet_size_;

  RtpHeaderExtensionMap rtp_header_extension_map_;
  size_t max_media_packet_header_;
  size_t max_padding_fec_packet_header_;

  // RTP variables
  uint32_t timestamp_offset_;
  // RID value to send in the RID or RepairedRID header extension.
  const std::string rid_;
  // MID value to send in the MID header extension.
  std::string mid_;
  // Should we send MID/RID even when ACKed? (see below).
  const bool always_send_mid_and_rid_;
  // Track if any ACK has been received on the SSRC and RTX SSRC to indicate
  // when to stop sending the MID and RID header extensions.
  bool ssrc_has_acked_;
  bool rtx_ssrc_has_acked_;
  // Maximum number of csrcs this sender is used with.
  size_t max_num_csrcs_ = 0;
  int rtx_;
  // Mapping rtx_payload_type_map_[associated] = rtx.
  std::map<int8_t, int8_t> rtx_payload_type_map_;
  bool supports_bwe_extension_;

  RateLimiter* const retransmission_rate_limiter_;

  AVE_DISALLOW_COPY_AND_ASSIGN(RTPSender);
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
