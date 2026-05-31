/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/src/util/frame_object.h"

#include <string.h>

#include <optional>
#include <utility>

namespace ave {
namespace media {
namespace rtp_rtcp {

RtpFrameObject::RtpFrameObject(uint16_t first_seq_num,
                               uint16_t last_seq_num,
                               bool marker_bit,
                               int times_nacked,
                               int64_t first_packet_received_time,
                               int64_t last_packet_received_time,
                               uint32_t rtp_timestamp,
                               int64_t ntp_time_ms,
                               const VideoSendTiming& timing,
                               uint8_t payload_type,
                               VideoCodecType codec,
                               VideoRotation rotation,
                               VideoContentType content_type,
                               const RTPVideoHeader& video_header,
                               const std::optional<ColorSpace>& color_space,
                               RtpPacketInfos packet_infos,
                               std::shared_ptr<EncodedImageBuffer> image_buffer)
    : image_buffer_(std::move(image_buffer)),
      codec_type_(codec),
      payload_type_(payload_type),
      rtp_timestamp_(rtp_timestamp),
      ntp_time_ms_(ntp_time_ms),
      first_seq_num_(first_seq_num),
      last_seq_num_(last_seq_num),
      last_packet_received_time_(last_packet_received_time),
      times_nacked_(times_nacked),
      rotation_(rotation),
      content_type_(content_type),
      color_space_(color_space),
      is_last_spatial_layer_(marker_bit) {
  rtp_video_header_ = video_header;

  // Set video dimensions from header
  encoded_width_ = rtp_video_header_.width;
  encoded_height_ = rtp_video_header_.height;

  // Extract CSRCs from the first packet info
  if (packet_infos.begin() != packet_infos.end()) {
    csrcs_ = packet_infos.begin()->csrcs();
  }

  // Store packet infos
  packet_infos_ = std::move(packet_infos);

  // Copy playout delay from header
  playout_delay_ = rtp_video_header_.playout_delay;

  // Set timing information
  if (timing.flags != VideoSendTiming::kInvalid) {
    // ntp_time_ms_ may be -1 if not estimated yet. This is not a problem,
    // as this will be dealt with at the time of reporting.
    timing_.encode_start_ms = ntp_time_ms_ + timing.encode_start_delta_ms;
    timing_.encode_finish_ms = ntp_time_ms_ + timing.encode_finish_delta_ms;
    timing_.packetization_finish_ms =
        ntp_time_ms_ + timing.packetization_finish_delta_ms;
    timing_.pacer_exit_ms = ntp_time_ms_ + timing.pacer_exit_delta_ms;
    timing_.network_timestamp_ms =
        ntp_time_ms_ + timing.network_timestamp_delta_ms;
    timing_.network2_timestamp_ms =
        ntp_time_ms_ + timing.network2_timestamp_delta_ms;
  }
  timing_.receive_start_ms = first_packet_received_time;
  timing_.receive_finish_ms = last_packet_received_time;
  timing_.flags = timing.flags;

  // Extract generic descriptor info if available
  if (rtp_video_header_.generic.has_value()) {
    const auto& generic = rtp_video_header_.generic.value();
    frame_id_ = generic.frame_id;
    spatial_index_ = generic.spatial_index;
    temporal_index_ = generic.temporal_index;
    dependencies_ = generic.dependencies;
  }
}

RtpFrameObject::~RtpFrameObject() = default;

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
