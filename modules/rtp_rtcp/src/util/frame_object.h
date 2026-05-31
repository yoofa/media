/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FRAME_OBJECT_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FRAME_OBJECT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <memory>

#include "media/foundation/color_space.h"
#include "media/foundation/video_codec_type.h"
#include "media/foundation/video_content_type.h"
#include "media/foundation/video_frame_type.h"
#include "media/foundation/video_rotation.h"
#include "media/modules/rtp_rtcp/api/encoded_image_buffer.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_infos.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Timing information for video frames.
struct FrameTiming {
  int64_t encode_start_ms = 0;
  int64_t encode_finish_ms = 0;
  int64_t packetization_finish_ms = 0;
  int64_t pacer_exit_ms = 0;
  int64_t network_timestamp_ms = 0;
  int64_t network2_timestamp_ms = 0;
  int64_t receive_start_ms = 0;
  int64_t receive_finish_ms = 0;
  uint8_t flags = 0;
};

// Playout delay for video frames.
struct PlayoutDelay {
  int min_ms = -1;
  int max_ms = -1;
};

// RtpFrameObject represents an assembled video frame from RTP packets.
// It combines RTP-specific metadata with the encoded frame data.
class RtpFrameObject {
 public:
  RtpFrameObject(uint16_t first_seq_num,
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
                 std::shared_ptr<EncodedImageBuffer> image_buffer);

  ~RtpFrameObject();

  // Accessors for RTP-related information
  uint16_t first_seq_num() const { return first_seq_num_; }
  uint16_t last_seq_num() const { return last_seq_num_; }
  int times_nacked() const { return times_nacked_; }
  bool delayed_by_retransmission() const { return times_nacked_ > 0; }

  // Frame information
  VideoFrameType frame_type() const { return rtp_video_header_.frame_type; }
  VideoCodecType codec_type() const { return codec_type_; }
  uint8_t payload_type() const { return payload_type_; }

  // Timestamp information
  uint32_t rtp_timestamp() const { return rtp_timestamp_; }
  int64_t ntp_time_ms() const { return ntp_time_ms_; }
  int64_t received_time() const { return last_packet_received_time_; }
  int64_t render_time() const { return render_time_ms_; }
  void set_render_time(int64_t render_time_ms) {
    render_time_ms_ = render_time_ms;
  }

  // Video properties
  uint16_t width() const { return encoded_width_; }
  uint16_t height() const { return encoded_height_; }
  VideoRotation rotation() const { return rotation_; }
  VideoContentType content_type() const { return content_type_; }

  // Color space
  const std::optional<ColorSpace>& color_space() const { return color_space_; }
  void set_color_space(const std::optional<ColorSpace>& color_space) {
    color_space_ = color_space;
  }

  // RTP video header
  const RTPVideoHeader& GetRtpVideoHeader() const { return rtp_video_header_; }

  // Data access
  const uint8_t* data() const {
    return image_buffer_ ? image_buffer_->data() : nullptr;
  }
  uint8_t* mutable_data() {
    return image_buffer_ ? image_buffer_->data() : nullptr;
  }
  size_t size() const { return image_buffer_ ? image_buffer_->size() : 0; }

  // Image buffer
  std::shared_ptr<EncodedImageBuffer> image_buffer() const {
    return image_buffer_;
  }

  // Packet info
  const RtpPacketInfos& packet_infos() const { return packet_infos_; }
  void SetPacketInfos(RtpPacketInfos packet_infos) {
    packet_infos_ = std::move(packet_infos);
  }

  // CSRCs (Contributing Sources)
  const std::vector<uint32_t>& Csrcs() const { return csrcs_; }

  // Timing
  const FrameTiming& timing() const { return timing_; }

  // Playout delay
  const std::optional<VideoPlayoutDelay>& playout_delay() const {
    return playout_delay_;
  }
  void set_playout_delay(const std::optional<VideoPlayoutDelay>& delay) {
    playout_delay_ = delay;
  }

  // Frame ID (for dependency tracking)
  std::optional<int64_t> frame_id() const { return frame_id_; }
  void set_frame_id(int64_t frame_id) { frame_id_ = frame_id; }

  // Spatial layer
  bool is_last_spatial_layer() const { return is_last_spatial_layer_; }
  int spatial_index() const { return spatial_index_; }
  void set_spatial_index(int index) { spatial_index_ = index; }

  // Temporal layer
  int temporal_index() const { return temporal_index_; }
  void set_temporal_index(int index) { temporal_index_ = index; }

  // Dependencies
  const std::vector<int64_t>& dependencies() const { return dependencies_; }
  void set_dependencies(std::vector<int64_t> deps) {
    dependencies_ = std::move(deps);
  }

  // Video frame tracking ID
  std::optional<uint16_t> video_frame_tracking_id() const {
    return rtp_video_header_.video_frame_tracking_id;
  }

  // Setters for sequence numbers (for reassembly adjustments)
  void SetFirstSeqNum(uint16_t first_seq_num) {
    first_seq_num_ = first_seq_num;
  }
  void SetLastSeqNum(uint16_t last_seq_num) { last_seq_num_ = last_seq_num; }

 private:
  // Reference to the encoded data
  std::shared_ptr<EncodedImageBuffer> image_buffer_;

  // RTP video header containing codec-specific info
  RTPVideoHeader rtp_video_header_;

  // Basic frame info
  VideoCodecType codec_type_;
  uint8_t payload_type_;
  uint32_t rtp_timestamp_;
  int64_t ntp_time_ms_;
  int64_t render_time_ms_ = 0;

  // Sequence numbers
  uint16_t first_seq_num_;
  uint16_t last_seq_num_;

  // Timing
  int64_t last_packet_received_time_;
  FrameTiming timing_;

  // Packet statistics
  int times_nacked_;

  // Video properties
  uint16_t encoded_width_ = 0;
  uint16_t encoded_height_ = 0;
  VideoRotation rotation_ = kVideoRotation_0;
  VideoContentType content_type_ = VideoContentType::UNSPECIFIED;
  std::optional<ColorSpace> color_space_;

  // RTP packet infos
  RtpPacketInfos packet_infos_;

  // Contributing sources
  std::vector<uint32_t> csrcs_;

  // Playout delay
  std::optional<VideoPlayoutDelay> playout_delay_;

  // Frame ID and dependencies (for frame dependency tracking)
  std::optional<int64_t> frame_id_;
  std::vector<int64_t> dependencies_;

  // Spatial/temporal layer info
  bool is_last_spatial_layer_ = true;
  int spatial_index_ = 0;
  int temporal_index_ = 0;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FRAME_OBJECT_H_
