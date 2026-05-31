/*
 * rtp_sender.cc - RTP Sender implementation
 *
 * Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 * Copyright (c) 2024 The aspect project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source tree.
 */

// NOTE: Include logging.h FIRST to ensure system headers (sstream, etc.)
// are included before any namespace declarations.
#include "base/logging.h"

#include "media/modules/rtp_rtcp/src/rtp/rtp_sender.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <span>
#include "base/checks.h"
#include "base/clock.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include "media/modules/rtp_rtcp/api/rtp_headers.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sender.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_interface.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_generic_frame_descriptor_extension.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extension_size.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extensions.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_history.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/util/byte_io.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

using std::span;

namespace {
constexpr size_t kMinAudioPaddingLength = 50;
constexpr size_t kRtpHeaderLength = 12;

// Min size needed to get payload padding from packet history.
constexpr int kMinPayloadPaddingBytes = 50;

// Determines how much larger a payload padding packet may be, compared to the
// requested padding size.
constexpr double kMaxPaddingSizeFactor = 3.0;

template <typename T>
constexpr size_t arraysize(const T& arr) {
  return sizeof(arr) / sizeof(arr[0]);
}

template <typename Extension>
constexpr RtpExtensionSize CreateExtensionSize() {
  return {Extension::kId, Extension::kValueSizeBytes};
}

template <typename Extension>
constexpr RtpExtensionSize CreateMaxExtensionSize() {
  return {Extension::kId, Extension::kMaxValueSizeBytes};
}

// Size info for header extensions that might be used in padding or FEC packets.
constexpr RtpExtensionSize kFecOrPaddingExtensionSizes[] = {
    CreateExtensionSize<AbsoluteSendTime>(),
    CreateExtensionSize<TransmissionOffset>(),
    CreateExtensionSize<TransportSequenceNumber>(),
    CreateExtensionSize<PlayoutDelayLimits>(),
    CreateMaxExtensionSize<RtpMid>(),
    CreateExtensionSize<VideoTimingExtension>(),
};

// Size info for header extensions that might be used in video packets.
constexpr RtpExtensionSize kVideoExtensionSizes[] = {
    CreateExtensionSize<AbsoluteSendTime>(),
    CreateExtensionSize<AbsoluteCaptureTimeExtension>(),
    CreateExtensionSize<TransmissionOffset>(),
    CreateExtensionSize<TransportSequenceNumber>(),
    CreateExtensionSize<PlayoutDelayLimits>(),
    CreateExtensionSize<VideoOrientation>(),
    CreateExtensionSize<VideoContentTypeExtension>(),
    CreateExtensionSize<VideoTimingExtension>(),
    CreateMaxExtensionSize<RtpStreamId>(),
    CreateMaxExtensionSize<RepairedRtpStreamId>(),
    CreateMaxExtensionSize<RtpMid>(),
    {RtpGenericFrameDescriptorExtension00::kId,
     RtpGenericFrameDescriptorExtension00::kMaxSizeBytes},
};

// Size info for header extensions that might be used in audio packets.
constexpr RtpExtensionSize kAudioExtensionSizes[] = {
    CreateExtensionSize<AbsoluteSendTime>(),
    CreateExtensionSize<AbsoluteCaptureTimeExtension>(),
    CreateExtensionSize<AudioLevelExtension>(),
    CreateExtensionSize<InbandComfortNoiseExtension>(),
    CreateExtensionSize<TransmissionOffset>(),
    CreateExtensionSize<TransportSequenceNumber>(),
    CreateMaxExtensionSize<RtpMid>(),
};

// Non-volatile extensions can be expected on all packets, if registered.
bool IsNonVolatile(RTPExtensionType type) {
  switch (type) {
    case kRtpExtensionTransmissionTimeOffset:
    case kRtpExtensionAudioLevel:
    case kRtpExtensionCsrcAudioLevel:
    case kRtpExtensionAbsoluteSendTime:
    case kRtpExtensionTransportSequenceNumber:
    case kRtpExtensionTransportSequenceNumber02:
    case kRtpExtensionRtpStreamId:
    case kRtpExtensionRepairedRtpStreamId:
    case kRtpExtensionMid:
    case kRtpExtensionGenericFrameDescriptor:
    case kRtpExtensionDependencyDescriptor:
      return true;
    case kRtpExtensionInbandComfortNoise:
    case kRtpExtensionAbsoluteCaptureTime:
    case kRtpExtensionVideoRotation:
    case kRtpExtensionPlayoutDelay:
    case kRtpExtensionVideoContentType:
    case kRtpExtensionVideoLayersAllocation:
    case kRtpExtensionVideoTiming:
    case kRtpExtensionColorSpace:
    case kRtpExtensionVideoFrameTrackingId:
    case kRtpExtensionCorruptionDetection:
      return false;
    case kRtpExtensionNone:
    case kRtpExtensionNumberOfExtensions:
      AVE_DCHECK(false);
      return false;
  }
  AVE_CHECK(false);
  return false;
}

bool HasBweExtension(const RtpHeaderExtensionMap& extensions_map) {
  return extensions_map.IsRegistered(kRtpExtensionTransportSequenceNumber) ||
         extensions_map.IsRegistered(kRtpExtensionTransportSequenceNumber02) ||
         extensions_map.IsRegistered(kRtpExtensionAbsoluteSendTime) ||
         extensions_map.IsRegistered(kRtpExtensionTransmissionTimeOffset);
}

}  // namespace

RTPSender::RTPSender(base::Clock* clock,
                     const RtpRtcpInterface::Configuration& config,
                     RtpPacketHistory* packet_history,
                     RtpPacketSender* packet_sender)
    : clock_(clock),
      random_(clock_->TimeInMicroseconds()),
      audio_configured_(config.audio),
      ssrc_(config.local_media_ssrc),
      rtx_ssrc_(config.rtx_send_ssrc),
      flexfec_ssrc_(config.fec_generator ? config.fec_generator->FecSsrc()
                                         : std::nullopt),
      packet_history_(packet_history),
      paced_sender_(packet_sender),
      sending_media_(true),
      max_packet_size_(IP_PACKET_SIZE - 28),
      rtp_header_extension_map_(config.extmap_allow_mixed),
      rid_(config.rid),
      always_send_mid_and_rid_(config.always_send_mid_and_rid),
      ssrc_has_acked_(false),
      rtx_ssrc_has_acked_(false),
      rtx_(kRtxOff),
      supports_bwe_extension_(false),
      retransmission_rate_limiter_(config.retransmission_rate_limiter) {
  timestamp_offset_ = random_.Rand<uint32_t>();

  AVE_DCHECK(paced_sender_);
  AVE_DCHECK(packet_history_);
  AVE_DCHECK(rid_.size() <= RtpStreamId::kMaxValueSizeBytes);

  UpdateHeaderSizes();
}

RTPSender::~RTPSender() = default;

std::span<const RtpExtensionSize> RTPSender::FecExtensionSizes() {
  return std::span<const RtpExtensionSize>(
      kFecOrPaddingExtensionSizes, arraysize(kFecOrPaddingExtensionSizes));
}

std::span<const RtpExtensionSize> RTPSender::VideoExtensionSizes() {
  return std::span<const RtpExtensionSize>(kVideoExtensionSizes,
                                           arraysize(kVideoExtensionSizes));
}

std::span<const RtpExtensionSize> RTPSender::AudioExtensionSizes() {
  return std::span<const RtpExtensionSize>(kAudioExtensionSizes,
                                           arraysize(kAudioExtensionSizes));
}

void RTPSender::SetExtmapAllowMixed(bool extmap_allow_mixed) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  rtp_header_extension_map_.SetExtmapAllowMixed(extmap_allow_mixed);
}

bool RTPSender::RegisterRtpHeaderExtension(std::string_view uri, int id) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  bool registered = rtp_header_extension_map_.RegisterByUri(id, uri);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
  UpdateHeaderSizes();
  return registered;
}

bool RTPSender::IsRtpHeaderExtensionRegistered(RTPExtensionType type) const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return rtp_header_extension_map_.IsRegistered(type);
}

void RTPSender::DeregisterRtpHeaderExtension(std::string_view uri) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  rtp_header_extension_map_.Deregister(uri);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
  UpdateHeaderSizes();
}

void RTPSender::SetMaxRtpPacketSize(size_t max_packet_size) {
  AVE_DCHECK(max_packet_size >= 100);
  AVE_DCHECK(max_packet_size <= IP_PACKET_SIZE);
  std::lock_guard<std::mutex> lock(send_mutex_);
  max_packet_size_ = max_packet_size;
}

size_t RTPSender::MaxRtpPacketSize() const {
  return max_packet_size_;
}

void RTPSender::SetRtxStatus(int mode) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  if (mode != kRtxOff &&
      (!rtx_ssrc_.has_value() || rtx_payload_type_map_.empty())) {
    AVE_LOG(LS_ERROR)
        << "Failed to enable RTX without RTX SSRC or payload types.";
    return;
  }
  rtx_ = mode;
}

int RTPSender::RtxStatus() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return rtx_;
}

void RTPSender::SetRtxPayloadType(int payload_type,
                                  int associated_payload_type) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  AVE_DCHECK(payload_type <= 127);
  AVE_DCHECK(associated_payload_type <= 127);
  if (payload_type < 0) {
    AVE_LOG(LS_ERROR) << "Invalid RTX payload type: " << payload_type << ".";
    return;
  }
  rtx_payload_type_map_[associated_payload_type] = payload_type;
}

int32_t RTPSender::ReSendPacket(uint16_t packet_id) {
  int32_t packet_size = 0;
  const bool rtx = (RtxStatus() & kRtxRetransmitted) > 0;

  std::unique_ptr<RtpPacketToSend> packet =
      packet_history_->GetPacketAndMarkAsPending(
          packet_id, [&](const RtpPacketToSend& stored_packet) {
            packet_size = stored_packet.size();
            std::unique_ptr<RtpPacketToSend> retransmit_packet;
            // TODO(user): Implement RateLimiter in media namespace
            // if (retransmission_rate_limiter_ &&
            //     !retransmission_rate_limiter_->TryUseRate(packet_size)) {
            //   return retransmit_packet;
            // }
            if (rtx) {
              retransmit_packet = BuildRtxPacket(stored_packet);
            } else {
              retransmit_packet =
                  std::make_unique<RtpPacketToSend>(stored_packet);
            }
            if (retransmit_packet) {
              retransmit_packet->set_retransmitted_sequence_number(
                  stored_packet.SequenceNumber());
              retransmit_packet->set_original_ssrc(stored_packet.Ssrc());
            }
            return retransmit_packet;
          });
  if (packet_size == 0) {
    AVE_DCHECK(!packet);
    return 0;
  }
  if (!packet) {
    return -1;
  }
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  packet->set_fec_protect_packet(false);
  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  packets.emplace_back(std::move(packet));
  paced_sender_->EnqueuePackets(std::move(packets));

  return packet_size;
}

void RTPSender::OnReceivedAckOnSsrc(int64_t) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  bool update_required = !ssrc_has_acked_;
  ssrc_has_acked_ = true;
  if (update_required) {
    UpdateHeaderSizes();
  }
}

void RTPSender::OnReceivedAckOnRtxSsrc(int64_t) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  bool update_required = !rtx_ssrc_has_acked_;
  rtx_ssrc_has_acked_ = true;
  if (update_required) {
    UpdateHeaderSizes();
  }
}

void RTPSender::OnReceivedNack(
    const std::vector<uint16_t>& nack_sequence_numbers,
    int64_t avg_rtt) {
  packet_history_->SetRtt(TimeDelta::Millis(5 + avg_rtt));
  for (uint16_t seq_no : nack_sequence_numbers) {
    const int32_t bytes_sent = ReSendPacket(seq_no);
    if (bytes_sent < 0) {
      AVE_LOG(LS_WARNING) << "Failed resending RTP packet " << seq_no
                          << ", Discard rest of packets.";
      break;
    }
  }
}

bool RTPSender::SupportsPadding() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return sending_media_ && supports_bwe_extension_;
}

bool RTPSender::SupportsRtxPayloadPadding() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return sending_media_ && supports_bwe_extension_ &&
         (rtx_ & kRtxRedundantPayloads);
}

std::vector<std::unique_ptr<RtpPacketToSend>> RTPSender::GeneratePadding(
    size_t target_size_bytes,
    bool media_has_been_sent,
    bool can_send_padding_on_media_ssrc) {
  std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
  size_t bytes_left = target_size_bytes;
  if (SupportsRtxPayloadPadding()) {
    while (bytes_left >= kMinPayloadPaddingBytes) {
      std::unique_ptr<RtpPacketToSend> packet =
          packet_history_->GetPayloadPaddingPacket(
              [&](const RtpPacketToSend& packet)
                  -> std::unique_ptr<RtpPacketToSend> {
                const size_t max_overshoot_bytes = static_cast<size_t>(
                    ((kMaxPaddingSizeFactor - 1.0) * target_size_bytes) + 0.5);
                if (packet.payload_size() + kRtxHeaderSize >
                    max_overshoot_bytes + bytes_left) {
                  return nullptr;
                }
                return BuildRtxPacket(packet);
              });
      if (!packet) {
        break;
      }
      bytes_left -= std::min(bytes_left, packet->payload_size());
      packet->set_packet_type(RtpPacketMediaType::kPadding);
      padding_packets.push_back(std::move(packet));
    }
  }

  std::lock_guard<std::mutex> lock(send_mutex_);
  if (!sending_media_) {
    return {};
  }

  size_t padding_bytes_in_packet;
  const size_t max_payload_size =
      max_packet_size_ - max_padding_fec_packet_header_;
  if (audio_configured_) {
    padding_bytes_in_packet =
        std::clamp<size_t>(bytes_left, kMinAudioPaddingLength,
                           std::min(max_payload_size, kMaxPaddingLength));
  } else {
    padding_bytes_in_packet = std::min(max_payload_size, kMaxPaddingLength);
  }

  while (bytes_left > 0) {
    auto padding_packet =
        std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_);
    padding_packet->set_packet_type(RtpPacketMediaType::kPadding);
    padding_packet->SetMarker(false);
    if (rtx_ == kRtxOff) {
      if (!can_send_padding_on_media_ssrc) {
        break;
      }
      padding_packet->SetSsrc(ssrc_);
      if (always_send_mid_and_rid_ || !ssrc_has_acked_) {
        if (!mid_.empty()) {
          padding_packet->SetExtension<RtpMid>(mid_);
        }
        if (!rid_.empty()) {
          padding_packet->SetExtension<RtpStreamId>(rid_);
        }
      }
    } else {
      if (!media_has_been_sent &&
          !(rtp_header_extension_map_.IsRegistered(AbsoluteSendTime::kId) ||
            rtp_header_extension_map_.IsRegistered(
                TransportSequenceNumber::kId))) {
        break;
      }
      AVE_DCHECK(rtx_ssrc_);
      AVE_DCHECK(!rtx_payload_type_map_.empty());
      padding_packet->SetSsrc(*rtx_ssrc_);
      padding_packet->SetPayloadType(rtx_payload_type_map_.begin()->second);
      if (always_send_mid_and_rid_ || !rtx_ssrc_has_acked_) {
        if (!mid_.empty()) {
          padding_packet->SetExtension<RtpMid>(mid_);
        }
        if (!rid_.empty()) {
          padding_packet->SetExtension<RepairedRtpStreamId>(rid_);
        }
      }
    }
    padding_packet->ReserveExtension<TransportSequenceNumber>();
    padding_packet->ReserveExtension<TransmissionOffset>();
    padding_packet->ReserveExtension<AbsoluteSendTime>();
    padding_packet->SetPadding(padding_bytes_in_packet);
    bytes_left -= std::min(bytes_left, padding_bytes_in_packet);
    padding_packets.push_back(std::move(padding_packet));
  }
  return padding_packets;
}

void RTPSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  AVE_DCHECK(!packets.empty());
  Timestamp now = clock_->CurrentTime();
  for (auto& packet : packets) {
    AVE_DCHECK(packet);
    AVE_CHECK(packet->packet_type().has_value())
        << "Packet type must be set before sending.";
    if (packet->capture_time() <= Timestamp::Zero()) {
      packet->set_capture_time(now);
    }
  }
  paced_sender_->EnqueuePackets(std::move(packets));
}

size_t RTPSender::FecOrPaddingPacketMaxRtpHeaderLength() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return max_padding_fec_packet_header_;
}

size_t RTPSender::ExpectedPerPacketOverhead() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return max_media_packet_header_;
}

std::unique_ptr<RtpPacketToSend> RTPSender::AllocatePacket(
    std::span<const uint32_t> csrcs) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  AVE_DCHECK(csrcs.size() <= kRtpCsrcSize);
  if (csrcs.size() > max_num_csrcs_) {
    max_num_csrcs_ = csrcs.size();
    UpdateHeaderSizes();
  }
  auto packet = std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_,
                                                  max_packet_size_);
  packet->SetSsrc(ssrc_);
  packet->SetCsrcs(csrcs);
  packet->ReserveExtension<AbsoluteSendTime>();
  packet->ReserveExtension<TransmissionOffset>();
  packet->ReserveExtension<TransportSequenceNumber>();

  if (always_send_mid_and_rid_ || !ssrc_has_acked_) {
    if (!mid_.empty()) {
      packet->SetExtension<RtpMid>(mid_);
    }
    if (!rid_.empty()) {
      packet->SetExtension<RtpStreamId>(rid_);
    }
  }
  return packet;
}

size_t RTPSender::RtxPacketOverhead() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  if (rtx_ == kRtxOff) {
    return 0;
  }
  size_t overhead = 0;
  if (!always_send_mid_and_rid_ && (!rtx_ssrc_has_acked_ && ssrc_has_acked_)) {
    static constexpr int kRtpExtensionHeaderSize = 2;
    if (!mid_.empty()) {
      overhead += (kRtpExtensionHeaderSize + mid_.size());
    }
    if (!rid_.empty()) {
      overhead += (kRtpExtensionHeaderSize + rid_.size());
    }
    overhead += 3;
  }
  overhead += kRtxHeaderSize;
  return overhead;
}

void RTPSender::SetSendingMediaStatus(bool enabled) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  sending_media_ = enabled;
}

bool RTPSender::SendingMedia() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return sending_media_;
}

bool RTPSender::IsAudioConfigured() const {
  return audio_configured_;
}

void RTPSender::SetTimestampOffset(uint32_t timestamp) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  timestamp_offset_ = timestamp;
}

uint32_t RTPSender::TimestampOffset() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  return timestamp_offset_;
}

void RTPSender::SetMid(std::string_view mid) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  AVE_DCHECK(mid.length() <= RtpMid::kMaxValueSizeBytes);
  mid_ = std::string(mid);
  UpdateHeaderSizes();
}

static void CopyHeaderAndExtensionsToRtxPacket(const RtpPacketToSend& packet,
                                               RtpPacketToSend* rtx_packet) {
  rtx_packet->SetMarker(packet.Marker());
  rtx_packet->SetTimestamp(packet.Timestamp());
  auto csrcs = packet.Csrcs();
  rtx_packet->SetCsrcs(std::span<const uint32_t>(csrcs.data(), csrcs.size()));
  for (int extension_num = kRtpExtensionNone + 1;
       extension_num < kRtpExtensionNumberOfExtensions; ++extension_num) {
    auto extension = static_cast<RTPExtensionType>(extension_num);
    if (extension == kRtpExtensionMid ||
        extension == kRtpExtensionRtpStreamId) {
      continue;
    }
    if (!packet.HasExtension(extension)) {
      continue;
    }
    std::span<const uint8_t> source = packet.FindExtension(extension);
    std::span<uint8_t> destination =
        rtx_packet->AllocateExtension(extension, source.size());
    if (destination.empty() || source.size() != destination.size()) {
      continue;
    }
    std::memcpy(destination.data(), source.data(), destination.size());
  }
}

std::unique_ptr<RtpPacketToSend> RTPSender::BuildRtxPacket(
    const RtpPacketToSend& packet) {
  std::unique_ptr<RtpPacketToSend> rtx_packet;
  {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!sending_media_)
      return nullptr;

    AVE_DCHECK(rtx_ssrc_);
    auto kv = rtx_payload_type_map_.find(packet.PayloadType());
    if (kv == rtx_payload_type_map_.end())
      return nullptr;

    rtx_packet = std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_,
                                                   max_packet_size_);
    rtx_packet->SetPayloadType(kv->second);
    rtx_packet->SetSsrc(*rtx_ssrc_);
    CopyHeaderAndExtensionsToRtxPacket(packet, rtx_packet.get());
    if (always_send_mid_and_rid_ || !rtx_ssrc_has_acked_) {
      if (!mid_.empty()) {
        rtx_packet->SetExtension<RtpMid>(mid_);
      }
      if (!rid_.empty()) {
        rtx_packet->SetExtension<RepairedRtpStreamId>(rid_);
      }
    }
  }
  AVE_DCHECK(rtx_packet);

  uint8_t* rtx_payload =
      rtx_packet->AllocatePayload(packet.payload_size() + kRtxHeaderSize);
  AVE_CHECK(rtx_payload);
  ByteWriter<uint16_t>::WriteBigEndian(rtx_payload, packet.SequenceNumber());
  auto payload = packet.payload();
  if (!payload.empty()) {
    memcpy(rtx_payload + kRtxHeaderSize, payload.data(), payload.size());
  }
  rtx_packet->set_capture_time(packet.capture_time());
  return rtx_packet;
}

void RTPSender::SetRtpState(const RtpState& rtp_state) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  timestamp_offset_ = rtp_state.start_timestamp;
  ssrc_has_acked_ = rtp_state.ssrc_has_acked;
  UpdateHeaderSizes();
}

RtpState RTPSender::GetRtpState() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  RtpState state;
  state.start_timestamp = timestamp_offset_;
  state.ssrc_has_acked = ssrc_has_acked_;
  return state;
}

void RTPSender::SetRtxRtpState(const RtpState& rtp_state) {
  std::lock_guard<std::mutex> lock(send_mutex_);
  rtx_ssrc_has_acked_ = rtp_state.ssrc_has_acked;
}

RtpState RTPSender::GetRtxRtpState() const {
  std::lock_guard<std::mutex> lock(send_mutex_);
  RtpState state;
  state.start_timestamp = timestamp_offset_;
  state.ssrc_has_acked = rtx_ssrc_has_acked_;
  return state;
}

void RTPSender::UpdateHeaderSizes() {
  const size_t rtp_header_length =
      kRtpHeaderLength + sizeof(uint32_t) * max_num_csrcs_;
  max_padding_fec_packet_header_ =
      rtp_header_length + RtpHeaderExtensionSize(kFecOrPaddingExtensionSizes,
                                                 rtp_header_extension_map_);

  const bool send_mid_rid_on_rtx =
      rtx_ssrc_.has_value() &&
      (always_send_mid_and_rid_ || !rtx_ssrc_has_acked_);
  const bool send_mid_rid = always_send_mid_and_rid_ || !ssrc_has_acked_;
  std::vector<RtpExtensionSize> non_volatile_extensions;
  for (auto& extension :
       audio_configured_ ? AudioExtensionSizes() : VideoExtensionSizes()) {
    if (IsNonVolatile(extension.type)) {
      switch (extension.type) {
        case RTPExtensionType::kRtpExtensionMid:
          if ((send_mid_rid || send_mid_rid_on_rtx) && !mid_.empty()) {
            non_volatile_extensions.push_back(extension);
          }
          break;
        case RTPExtensionType::kRtpExtensionRtpStreamId:
          if (send_mid_rid && !rid_.empty()) {
            non_volatile_extensions.push_back(extension);
          }
          break;
        case RTPExtensionType::kRtpExtensionRepairedRtpStreamId:
          if (send_mid_rid_on_rtx && !send_mid_rid && !rid_.empty()) {
            non_volatile_extensions.push_back(extension);
          }
          break;
        default:
          non_volatile_extensions.push_back(extension);
      }
    }
  }
  max_media_packet_header_ =
      rtp_header_length +
      RtpHeaderExtensionSize(
          std::span<const RtpExtensionSize>(non_volatile_extensions.data(),
                                            non_volatile_extensions.size()),
          rtp_header_extension_map_);
  if (rtx_ssrc_.has_value()) {
    max_media_packet_header_ += kRtxHeaderSize;
  }
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
