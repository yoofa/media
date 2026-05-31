/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_received.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extensions.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

RtpPacketReceived::RtpPacketReceived() = default;
RtpPacketReceived::RtpPacketReceived(
    const ExtensionManager* extensions,
    base::Timestamp arrival_time /*= base::Timestamp::MinusInfinity()*/)
    : RtpPacket(extensions), arrival_time_(arrival_time) {}
RtpPacketReceived::RtpPacketReceived(const RtpPacketReceived& packet) = default;
RtpPacketReceived::RtpPacketReceived(RtpPacketReceived&& packet) = default;

RtpPacketReceived& RtpPacketReceived::operator=(
    const RtpPacketReceived& packet) = default;
RtpPacketReceived& RtpPacketReceived::operator=(RtpPacketReceived&& packet) =
    default;

RtpPacketReceived::~RtpPacketReceived() = default;

void RtpPacketReceived::GetHeader(RTPHeader* header) const {
  header->markerBit = RtpPacket::Marker();
  header->payloadType = RtpPacket::PayloadType();
  header->sequenceNumber = RtpPacket::SequenceNumber();
  header->timestamp = RtpPacket::Timestamp();
  header->ssrc = RtpPacket::Ssrc();
  std::vector<uint32_t> csrcs = RtpPacket::Csrcs();
  header->numCSRCs = base::dchecked_cast<uint8_t>(csrcs.size());
  for (size_t i = 0; i < csrcs.size(); ++i) {
    header->arrOfCSRCs[i] = csrcs[i];
  }
  header->paddingLength = RtpPacket::padding_size();
  header->headerLength = RtpPacket::headers_size();
  header->extension.hasTransmissionTimeOffset =
      RtpPacket::GetExtension<TransmissionOffset>(
          &header->extension.transmissionTimeOffset);
  header->extension.hasAbsoluteSendTime =
      RtpPacket::GetExtension<AbsoluteSendTime>(
          &header->extension.absoluteSendTime);
  header->extension.absolute_capture_time =
      RtpPacket::GetExtension<AbsoluteCaptureTimeExtension>();
  header->extension.hasTransportSequenceNumber =
      RtpPacket::GetExtension<TransportSequenceNumberV2>(
          &header->extension.transportSequenceNumber,
          &header->extension.feedback_request) ||
      RtpPacket::GetExtension<TransportSequenceNumber>(
          &header->extension.transportSequenceNumber);
  header->extension.set_audio_level(
      RtpPacket::GetExtension<AudioLevelExtension>());
  header->extension.hasVideoRotation =
      RtpPacket::GetExtension<VideoOrientation>(
          &header->extension.videoRotation);
  header->extension.hasVideoContentType =
      RtpPacket::GetExtension<VideoContentTypeExtension>(
          &header->extension.videoContentType);
  header->extension.has_video_timing =
      RtpPacket::GetExtension<VideoTimingExtension>(
          &header->extension.video_timing);
  RtpPacket::GetExtension<RtpStreamId>(&header->extension.stream_id);
  RtpPacket::GetExtension<RepairedRtpStreamId>(
      &header->extension.repaired_stream_id);
  RtpPacket::GetExtension<RtpMid>(&header->extension.mid);
  RtpPacket::GetExtension<PlayoutDelayLimits>(&header->extension.playout_delay);
  // TODO: Add ColorSpaceExtension support
  // header->extension.color_space =
  //     RtpPacket::GetExtension<ColorSpaceExtension>();
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
