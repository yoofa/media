/*
 * video_rtp_depacketizer_h264.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_h264.h"
#include <span>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/buffer.h"
#include "base/checks.h"
#include "base/logging.h"
#include "media/foundation/h264/h264_common.h"
#include "media/modules/rtp_rtcp/src/util/byte_io.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format_h264.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace {

constexpr size_t kNalHeaderSize = 1;
constexpr size_t kFuAHeaderSize = 2;
constexpr size_t kLengthFieldSize = 2;

std::vector<std::span<const uint8_t>> ParseStapA(
    std::span<const uint8_t> data) {
  std::vector<std::span<const uint8_t>> nal_units;

  if (data.size() < kNalHeaderSize) {
    return nal_units;
  }

  size_t offset = kNalHeaderSize;  // Skip STAP-A header

  while (offset + kLengthFieldSize <= data.size()) {
    uint16_t nalu_size = ByteReader<uint16_t>::ReadBigEndian(&data[offset]);
    offset += kLengthFieldSize;

    if (nalu_size == 0 || offset + nalu_size > data.size()) {
      return {};
    }
    nal_units.emplace_back(data.data() + offset, nalu_size);
    offset += nalu_size;
  }
  return nal_units;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ProcessStapAOrSingleNalu(
    base::CopyOnWriteBuffer rtp_payload) {
  std::span<const uint8_t> payload_data(rtp_payload.cdata(),
                                        rtp_payload.size());
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      std::in_place);
  parsed_payload->video_payload = rtp_payload;
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = VideoCodecType::kVideoCodecH264;
  parsed_payload->video_header.simulcastIdx = 0;
  parsed_payload->video_header.is_first_packet_in_frame = false;

  RTPVideoHeaderH264 h264_header;

  uint8_t nal_type = payload_data[0] & kH264TypeMask;
  std::vector<std::span<const uint8_t>> nal_units;
  if (nal_type == H264::NaluType::kStapA) {
    nal_units = ParseStapA(payload_data);
    if (nal_units.empty()) {
      AVE_LOG(LS_ERROR) << "Incorrect StapA packet.";
      return std::nullopt;
    }
    h264_header.packetization_type = kH264StapA;
    h264_header.nalu_type = nal_units[0][0] & kH264TypeMask;
  } else {
    h264_header.packetization_type = kH264SingleNalu;
    h264_header.nalu_type = nal_type;
    nal_units.push_back(payload_data);
  }

  parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;

  for (const std::span<const uint8_t>& nal_unit : nal_units) {
    NaluInfo nalu{};
    nalu.type = nal_unit[0] & kH264TypeMask;
    nalu.sps_id = -1;
    nalu.pps_id = -1;

    if (nal_unit.size() <= H264::kNaluTypeSize) {
      AVE_LOG(LS_WARNING) << "Skipping empty NAL unit.";
      continue;
    }

    switch (nalu.type) {
      case H264::NaluType::kSps:
        parsed_payload->video_header.frame_type =
            VideoFrameType::kVideoFrameKey;
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      case H264::NaluType::kPps:
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      case H264::NaluType::kIdr:
        parsed_payload->video_header.frame_type =
            VideoFrameType::kVideoFrameKey;
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      case H264::NaluType::kSlice:
        // For slice, check if it's the first slice in frame
        // (simplified - would need slice header parsing for accuracy)
        break;
      case H264::NaluType::kAud:
      case H264::NaluType::kSei:
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      case H264::NaluType::kEndOfSequence:
      case H264::NaluType::kEndOfStream:
      case H264::NaluType::kFiller:
        break;
      case H264::NaluType::kStapA:
      case H264::NaluType::kFuA:
        AVE_LOG(LS_WARNING) << "Unexpected STAP-A or FU-A received.";
        return std::nullopt;
    }

    h264_header.nalus.push_back(nalu);
  }

  parsed_payload->video_header.video_type_header = h264_header;
  return parsed_payload;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ParseFuaNalu(
    base::CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.size() < kFuAHeaderSize) {
    AVE_LOG(LS_ERROR) << "FU-A NAL units truncated.";
    return std::nullopt;
  }
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      std::in_place);
  uint8_t fnri = rtp_payload.cdata()[0] & (kH264FBit | kH264NriMask);
  uint8_t original_nal_type = rtp_payload.cdata()[1] & kH264TypeMask;
  bool first_fragment = (rtp_payload.cdata()[1] & kH264SBit) > 0;
  bool is_first_packet_in_frame = false;
  NaluInfo nalu{};
  nalu.type = original_nal_type;
  nalu.sps_id = -1;
  nalu.pps_id = -1;

  RTPVideoHeaderH264 h264_header;

  if (first_fragment) {
    if (original_nal_type == H264::NaluType::kIdr ||
        original_nal_type == H264::NaluType::kSlice) {
      // For first fragment, we might need slice header parsing
      // Simplified: assume first fragment of IDR/Slice is first in frame
      if (original_nal_type == H264::NaluType::kIdr) {
        is_first_packet_in_frame = true;
      }
    }
    uint8_t original_nal_header = fnri | original_nal_type;
    rtp_payload =
        rtp_payload.Slice(kNalHeaderSize, rtp_payload.size() - kNalHeaderSize);
    rtp_payload.MutableData()[0] = original_nal_header;
    parsed_payload->video_payload = std::move(rtp_payload);
  } else {
    parsed_payload->video_payload =
        rtp_payload.Slice(kFuAHeaderSize, rtp_payload.size() - kFuAHeaderSize);
  }

  if (original_nal_type == H264::NaluType::kIdr) {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameKey;
  } else {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  }
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = VideoCodecType::kVideoCodecH264;
  parsed_payload->video_header.simulcastIdx = 0;
  parsed_payload->video_header.is_first_packet_in_frame =
      is_first_packet_in_frame;

  h264_header.packetization_type = kH264FuA;
  h264_header.nalu_type = original_nal_type;
  if (first_fragment) {
    h264_header.nalus = {nalu};
  }
  parsed_payload->video_header.video_type_header = h264_header;
  return parsed_payload;
}

}  // namespace

std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerH264::Parse(base::CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.empty()) {
    AVE_LOG(LS_ERROR) << "Empty payload.";
    return std::nullopt;
  }

  uint8_t nal_type = rtp_payload.cdata()[0] & kH264TypeMask;

  if (nal_type == H264::NaluType::kFuA) {
    // Fragmented NAL units (FU-A).
    return ParseFuaNalu(std::move(rtp_payload));
  }  // We handle STAP-A and single NALU's the same way here. The jitter buffer
  // will depacketize the STAP-A into NAL units later.
  return ProcessStapAOrSingleNalu(std::move(rtp_payload));
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
