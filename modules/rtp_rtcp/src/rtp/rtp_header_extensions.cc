/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extensions.h"
#include <span>

#include <cstring>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "base/checks.h"
#include "media/modules/rtp_rtcp/api/rtp_headers.h"
#include "media/modules/rtp_rtcp/src/util/byte_io.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Absolute send time in RTP streams.
bool AbsoluteSendTime::Parse(std::span<const uint8_t> data,
                             uint32_t* time_24bits) {
  if (data.size() != 3) {
    return false;
  }
  *time_24bits = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
  return true;
}

bool AbsoluteSendTime::Write(std::span<uint8_t> data, uint32_t time_24bits) {
  AVE_DCHECK_EQ(data.size(), 3);
  AVE_DCHECK_LE(time_24bits, 0x00FFFFFF);
  ByteWriter<uint32_t, 3>::WriteBigEndian(data.data(), time_24bits);
  return true;
}

// Absolute Capture Time
bool AbsoluteCaptureTimeExtension::Parse(std::span<const uint8_t> data,
                                         AbsoluteCaptureTime* extension) {
  if (data.size() != kValueSizeBytes &&
      data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
    return false;
  }

  extension->absolute_capture_timestamp =
      ByteReader<uint64_t>::ReadBigEndian(data.data());

  if (data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
    extension->estimated_capture_clock_offset =
        ByteReader<int64_t>::ReadBigEndian(data.data() + 8);
  }

  return true;
}

size_t AbsoluteCaptureTimeExtension::ValueSize(
    const AbsoluteCaptureTime& extension) {
  if (extension.estimated_capture_clock_offset != std::nullopt) {
    return kValueSizeBytes;
  }
  return kValueSizeBytesWithoutEstimatedCaptureClockOffset;
}

bool AbsoluteCaptureTimeExtension::Write(std::span<uint8_t> data,
                                         const AbsoluteCaptureTime& extension) {
  AVE_DCHECK_EQ(data.size(), ValueSize(extension));

  ByteWriter<uint64_t>::WriteBigEndian(data.data(),
                                       extension.absolute_capture_timestamp);

  if (data.size() != kValueSizeBytesWithoutEstimatedCaptureClockOffset) {
    ByteWriter<int64_t>::WriteBigEndian(
        data.data() + 8, extension.estimated_capture_clock_offset.value());
  }

  return true;
}

// Audio Level Extension
bool AudioLevelExtension::Parse(std::span<const uint8_t> data,
                                AudioLevel* extension) {
  if (data.size() != 1) {
    return false;
  }
  bool voice_activity = (data[0] & 0x80) != 0;
  int32_t audio_level = data[0] & 0x7F;
  *extension = AudioLevel(voice_activity, audio_level);
  return true;
}

bool AudioLevelExtension::Write(std::span<uint8_t> data,
                                const AudioLevel& extension) {
  AVE_DCHECK_EQ(data.size(), 1);
  AVE_CHECK_GE(extension.level(), 0);
  AVE_CHECK_LE(extension.level(), 0x7f);
  data[0] = (extension.voice_activity() ? 0x80 : 0x00) | extension.level();
  return true;
}

// CSRC Audio Level
bool CsrcAudioLevel::Parse(std::span<const uint8_t> data,
                           std::vector<uint8_t>* csrc_audio_levels) {
  if (data.size() > kRtpCsrcSize) {
    return false;
  }
  csrc_audio_levels->resize(data.size());
  for (size_t i = 0; i < data.size(); i++) {
    (*csrc_audio_levels)[i] = data[i] & 0x7F;
  }
  return true;
}

size_t CsrcAudioLevel::ValueSize(std::span<const uint8_t> csrc_audio_levels) {
  return csrc_audio_levels.size();
}

bool CsrcAudioLevel::Write(std::span<uint8_t> data,
                           std::span<const uint8_t> csrc_audio_levels) {
  AVE_CHECK_LE(csrc_audio_levels.size(), kRtpCsrcSize);
  if (csrc_audio_levels.size() != data.size()) {
    return false;
  }
  for (size_t i = 0; i < csrc_audio_levels.size(); i++) {
    data[i] = csrc_audio_levels[i] & 0x7F;
  }
  return true;
}

// Transmission Offset
bool TransmissionOffset::Parse(std::span<const uint8_t> data,
                               int32_t* rtp_time) {
  if (data.size() != 3) {
    return false;
  }
  *rtp_time = ByteReader<int32_t, 3>::ReadBigEndian(data.data());
  return true;
}

bool TransmissionOffset::Write(std::span<uint8_t> data, int32_t rtp_time) {
  AVE_DCHECK_EQ(data.size(), 3);
  AVE_DCHECK_LE(rtp_time, 0x00ffffff);
  ByteWriter<int32_t, 3>::WriteBigEndian(data.data(), rtp_time);
  return true;
}

// TransportSequenceNumber
bool TransportSequenceNumber::Parse(std::span<const uint8_t> data,
                                    uint16_t* transport_sequence_number) {
  if (data.size() != kValueSizeBytes) {
    return false;
  }
  *transport_sequence_number = ByteReader<uint16_t>::ReadBigEndian(data.data());
  return true;
}

bool TransportSequenceNumber::Write(std::span<uint8_t> data,
                                    uint16_t transport_sequence_number) {
  AVE_DCHECK_EQ(data.size(), ValueSize(transport_sequence_number));
  ByteWriter<uint16_t>::WriteBigEndian(data.data(), transport_sequence_number);
  return true;
}

// TransportSequenceNumberV2
bool TransportSequenceNumberV2::Parse(
    std::span<const uint8_t> data,
    uint16_t* transport_sequence_number,
    std::optional<FeedbackRequest>* feedback_request) {
  if (data.size() != kValueSizeBytes &&
      data.size() != kValueSizeBytesWithoutFeedbackRequest) {
    return false;
  }

  *transport_sequence_number = ByteReader<uint16_t>::ReadBigEndian(data.data());

  *feedback_request = std::nullopt;
  if (data.size() == kValueSizeBytes) {
    uint16_t feedback_request_raw =
        ByteReader<uint16_t>::ReadBigEndian(data.data() + 2);
    bool include_timestamps =
        (feedback_request_raw & kIncludeTimestampsBit) != 0;
    uint16_t sequence_count = feedback_request_raw & ~kIncludeTimestampsBit;

    if (sequence_count != 0) {
      *feedback_request = {
          .include_timestamps = include_timestamps,
          .sequence_count = static_cast<int32_t>(sequence_count)};
    }
  }
  return true;
}

bool TransportSequenceNumberV2::Write(
    std::span<uint8_t> data,
    uint16_t transport_sequence_number,
    const std::optional<FeedbackRequest>& feedback_request) {
  AVE_DCHECK_EQ(data.size(),
                ValueSize(transport_sequence_number, feedback_request));

  ByteWriter<uint16_t>::WriteBigEndian(data.data(), transport_sequence_number);

  if (feedback_request) {
    AVE_DCHECK_GE(feedback_request->sequence_count, 0);
    AVE_DCHECK_LT(feedback_request->sequence_count, kIncludeTimestampsBit);
    uint16_t feedback_request_raw =
        feedback_request->sequence_count |
        (feedback_request->include_timestamps ? kIncludeTimestampsBit : 0);
    ByteWriter<uint16_t>::WriteBigEndian(data.data() + 2, feedback_request_raw);
  }
  return true;
}

// Helper function for video rotation conversion
static VideoRotation ConvertCVOByteToVideoRotation(uint8_t cvo_byte) {
  // CVO byte format: 0 0 0 0 C F R R
  // R R = 0: 0°, 1: 90°, 2: 180°, 3: 270°
  switch (cvo_byte & 0x03) {
    case 0:
      return kVideoRotation_0;
    case 1:
      return kVideoRotation_90;
    case 2:
      return kVideoRotation_180;
    case 3:
      return kVideoRotation_270;
    default:
      return kVideoRotation_0;
  }
}

static uint8_t ConvertVideoRotationToCVOByte(VideoRotation rotation) {
  switch (rotation) {
    case kVideoRotation_0:
      return 0;
    case kVideoRotation_90:
      return 1;
    case kVideoRotation_180:
      return 2;
    case kVideoRotation_270:
      return 3;
  }
  return 0;
}

// Video Orientation
bool VideoOrientation::Parse(std::span<const uint8_t> data,
                             VideoRotation* rotation) {
  if (data.size() != 1) {
    return false;
  }
  *rotation = ConvertCVOByteToVideoRotation(data[0]);
  return true;
}

bool VideoOrientation::Write(std::span<uint8_t> data, VideoRotation rotation) {
  AVE_DCHECK_EQ(data.size(), 1);
  data[0] = ConvertVideoRotationToCVOByte(rotation);
  return true;
}

bool VideoOrientation::Parse(std::span<const uint8_t> data, uint8_t* value) {
  if (data.size() != 1) {
    return false;
  }
  *value = data[0];
  return true;
}

bool VideoOrientation::Write(std::span<uint8_t> data, uint8_t value) {
  AVE_DCHECK_EQ(data.size(), 1);
  data[0] = value;
  return true;
}

// Playout Delay Limits
bool PlayoutDelayLimits::Parse(std::span<const uint8_t> data,
                               VideoPlayoutDelay* playout_delay) {
  AVE_DCHECK(playout_delay);
  if (data.size() != 3) {
    return false;
  }
  uint32_t raw = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
  uint16_t min_raw = (raw >> 12);
  uint16_t max_raw = (raw & 0xfff);
  playout_delay->min_ms = min_raw * kGranularityMs;
  playout_delay->max_ms = max_raw * kGranularityMs;
  return true;
}

bool PlayoutDelayLimits::Write(std::span<uint8_t> data,
                               const VideoPlayoutDelay& playout_delay) {
  AVE_DCHECK_EQ(data.size(), 3);

  int64_t min_delay = playout_delay.min_ms / kGranularityMs;
  int64_t max_delay = playout_delay.max_ms / kGranularityMs;

  AVE_DCHECK_GE(min_delay, 0);
  AVE_DCHECK_LT(min_delay, 1 << 12);
  AVE_DCHECK_GE(max_delay, 0);
  AVE_DCHECK_LT(max_delay, 1 << 12);

  ByteWriter<uint32_t, 3>::WriteBigEndian(data.data(),
                                          (min_delay << 12) | max_delay);
  return true;
}

// Video Content Type Extension
bool VideoContentTypeExtension::Parse(std::span<const uint8_t> data,
                                      VideoContentType* content_type) {
  if (data.size() == 1 && data[0] <= 1) {
    *content_type = static_cast<VideoContentType>(data[0] & 0x1);
    return true;
  }
  return false;
}

bool VideoContentTypeExtension::Write(std::span<uint8_t> data,
                                      VideoContentType content_type) {
  AVE_DCHECK_EQ(data.size(), 1);
  data[0] = static_cast<uint8_t>(content_type);
  return true;
}

// Video Timing Extension
bool VideoTimingExtension::Parse(std::span<const uint8_t> data,
                                 VideoSendTiming* timing) {
  AVE_DCHECK(timing);
  ptrdiff_t off = 0;
  switch (data.size()) {
    case kValueSizeBytes - 1:
      timing->flags = 0;
      off = 1;  // Old wire format without the flags field.
      break;
    case kValueSizeBytes:
      timing->flags = ByteReader<uint8_t>::ReadBigEndian(data.data());
      break;
    default:
      return false;
  }

  timing->encode_start_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + kEncodeStartDeltaOffset - off);
  timing->encode_finish_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + kEncodeFinishDeltaOffset - off);
  timing->packetization_finish_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + kPacketizationFinishDeltaOffset - off);
  timing->pacer_exit_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + kPacerExitDeltaOffset - off);
  timing->network_timestamp_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + kNetworkTimestampDeltaOffset - off);
  timing->network2_timestamp_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + kNetwork2TimestampDeltaOffset - off);
  return true;
}

bool VideoTimingExtension::Write(std::span<uint8_t> data,
                                 const VideoSendTiming& timing) {
  AVE_DCHECK_EQ(data.size(), 1 + 2 * 6);
  ByteWriter<uint8_t>::WriteBigEndian(data.data() + kFlagsOffset, timing.flags);
  ByteWriter<uint16_t>::WriteBigEndian(data.data() + kEncodeStartDeltaOffset,
                                       timing.encode_start_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(data.data() + kEncodeFinishDeltaOffset,
                                       timing.encode_finish_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + kPacketizationFinishDeltaOffset,
      timing.packetization_finish_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(data.data() + kPacerExitDeltaOffset,
                                       timing.pacer_exit_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + kNetworkTimestampDeltaOffset,
      timing.network_timestamp_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + kNetwork2TimestampDeltaOffset,
      timing.network2_timestamp_delta_ms);
  return true;
}

bool VideoTimingExtension::Write(std::span<uint8_t> data,
                                 uint16_t time_delta_ms,
                                 uint8_t offset) {
  AVE_DCHECK_GE(data.size(), offset + 2);
  AVE_DCHECK_LE(offset, kValueSizeBytes - sizeof(uint16_t));
  ByteWriter<uint16_t>::WriteBigEndian(data.data() + offset, time_delta_ms);
  return true;
}

// Base RTP String Extension
bool BaseRtpStringExtension::Parse(std::span<const uint8_t> data,
                                   std::string* str) {
  if (data.empty() || data[0] == 0) {  // Valid string extension can't be empty.
    return false;
  }
  const char* cstr = reinterpret_cast<const char*>(data.data());
  str->assign(cstr, strnlen(cstr, data.size()));
  AVE_DCHECK(!str->empty());
  return true;
}

bool BaseRtpStringExtension::Write(std::span<uint8_t> data,
                                   std::string_view str) {
  if (str.size() > kMaxValueSizeBytes) {
    return false;
  }
  AVE_DCHECK_EQ(data.size(), str.size());
  AVE_DCHECK_GE(str.size(), 1);
  memcpy(data.data(), str.data(), str.size());
  return true;
}

// Inband Comfort Noise Extension
bool InbandComfortNoiseExtension::Parse(std::span<const uint8_t> data,
                                        std::optional<uint8_t>* level) {
  if (data.size() != kValueSizeBytes) {
    return false;
  }
  *level = (data[0] & 0b1000'0000) != 0
               ? std::nullopt
               : std::make_optional(data[0] & 0b0111'1111);
  return true;
}

bool InbandComfortNoiseExtension::Write(std::span<uint8_t> data,
                                        std::optional<uint8_t> level) {
  AVE_DCHECK_EQ(data.size(), kValueSizeBytes);
  data[0] = 0b0000'0000;
  if (level) {
    if (*level > 127) {
      return false;
    }
    data[0] = 0b1000'0000 | *level;
  }
  return true;
}

// VideoFrameTrackingIdExtension
bool VideoFrameTrackingIdExtension::Parse(std::span<const uint8_t> data,
                                          uint16_t* video_frame_tracking_id) {
  if (data.size() != kValueSizeBytes) {
    return false;
  }
  *video_frame_tracking_id = ByteReader<uint16_t>::ReadBigEndian(data.data());
  return true;
}

bool VideoFrameTrackingIdExtension::Write(std::span<uint8_t> data,
                                          uint16_t video_frame_tracking_id) {
  AVE_DCHECK_EQ(data.size(), kValueSizeBytes);
  ByteWriter<uint16_t>::WriteBigEndian(data.data(), video_frame_tracking_id);
  return true;
}

// ColorSpaceExtension
uint8_t ColorSpaceExtension::CombineRangeAndChromaSiting(
    ColorSpace::RangeID range,
    ColorSpace::ChromaSiting chroma_siting_horizontal,
    ColorSpace::ChromaSiting chroma_siting_vertical) {
  return (static_cast<uint8_t>(range) << 4) |
         (static_cast<uint8_t>(chroma_siting_horizontal) << 2) |
         static_cast<uint8_t>(chroma_siting_vertical);
}

bool ColorSpaceExtension::Parse(std::span<const uint8_t> data,
                                ColorSpace* color_space) {
  if (data.size() != kValueSizeBytes &&
      data.size() != kValueSizeBytesWithHdrMetadata) {
    return false;
  }

  size_t offset = 0;
  // Read primaries, transfer, matrix, range/chroma siting
  if (!color_space->set_primaries_from_uint8(data[offset++])) {
    return false;
  }
  if (!color_space->set_transfer_from_uint8(data[offset++])) {
    return false;
  }
  if (!color_space->set_matrix_from_uint8(data[offset++])) {
    return false;
  }

  uint8_t range_and_chroma_siting = data[offset++];
  if (!color_space->set_range_from_uint8(range_and_chroma_siting >> 4)) {
    return false;
  }
  if (!color_space->set_chroma_siting_horizontal_from_uint8(
          (range_and_chroma_siting >> 2) & 0x3)) {
    return false;
  }
  if (!color_space->set_chroma_siting_vertical_from_uint8(
          range_and_chroma_siting & 0x3)) {
    return false;
  }

  // Read HDR metadata if present
  if (data.size() == kValueSizeBytesWithHdrMetadata) {
    HdrMetadata hdr_metadata;
    offset += ParseHdrMetadata(data.subspan(offset), &hdr_metadata);
    color_space->set_hdr_metadata(&hdr_metadata);
  } else {
    color_space->set_hdr_metadata(nullptr);
  }

  return true;
}

bool ColorSpaceExtension::Write(std::span<uint8_t> data,
                                const ColorSpace& color_space) {
  size_t expected_size = color_space.hdr_metadata()
                             ? kValueSizeBytesWithHdrMetadata
                             : kValueSizeBytes;
  if (data.size() != expected_size) {
    return false;
  }

  size_t offset = 0;
  data[offset++] = static_cast<uint8_t>(color_space.primaries());
  data[offset++] = static_cast<uint8_t>(color_space.transfer());
  data[offset++] = static_cast<uint8_t>(color_space.matrix());
  data[offset++] = CombineRangeAndChromaSiting(
      color_space.range(), color_space.chroma_siting_horizontal(),
      color_space.chroma_siting_vertical());

  if (color_space.hdr_metadata()) {
    offset +=
        WriteHdrMetadata(data.subspan(offset), *color_space.hdr_metadata());
  }

  return true;
}

size_t ColorSpaceExtension::ParseHdrMetadata(std::span<const uint8_t> data,
                                             HdrMetadata* hdr_metadata) {
  size_t offset = 0;
  // Parse mastering metadata if present
  hdr_metadata->mastering_metadata.luminance_max =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kLuminanceMaxDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.luminance_min =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kLuminanceMinDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.primary_r.x =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.primary_r.y =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.primary_g.x =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.primary_g.y =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.primary_b.x =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.primary_b.y =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.white_point.x =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->mastering_metadata.white_point.y =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset) *
      (1.0f / kChromaticityDenominator);
  offset += 2;

  hdr_metadata->max_content_light_level =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset);
  offset += 2;

  hdr_metadata->max_frame_average_light_level =
      ByteReader<uint16_t>::ReadBigEndian(data.data() + offset);
  offset += 2;

  return offset;
}

size_t ColorSpaceExtension::WriteHdrMetadata(std::span<uint8_t> data,
                                             const HdrMetadata& hdr_metadata) {
  size_t offset = 0;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.luminance_max *
                            kLuminanceMaxDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.luminance_min *
                            kLuminanceMinDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.primary_r.x *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.primary_r.y *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.primary_g.x *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.primary_g.y *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.primary_b.x *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.primary_b.y *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.white_point.x *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.mastering_metadata.white_point.y *
                            kChromaticityDenominator));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.max_content_light_level));
  offset += 2;

  ByteWriter<uint16_t>::WriteBigEndian(
      data.data() + offset,
      static_cast<uint16_t>(hdr_metadata.max_frame_average_light_level));
  offset += 2;

  return offset;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
