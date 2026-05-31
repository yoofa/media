/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_HEADER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_HEADER_H_

#include <bitset>
#include <cstdint>
#include <optional>
#include <variant>

#include "media/foundation/color_space.h"
#include "media/foundation/h264/h264_globals.h"
#include "media/foundation/video_codec_type.h"
#include "media/foundation/video_content_type.h"
#include "media/foundation/video_frame_type.h"
#include "media/foundation/video_rotation.h"
#include "media/foundation/vp8/vp8_globals.h"
#include "media/foundation/vp9/vp9_globals.h"
#include "media/modules/rtp_rtcp/api/rtp_headers.h"
#include "media/modules/rtp_rtcp/src/util/dependency_descriptor.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Details passed in the rtp payload for legacy generic rtp packetizer.
struct RTPVideoHeaderLegacyGeneric {
  uint16_t picture_id;
};

using RTPVideoTypeHeader = ::std::variant<::std::monostate,
                                          RTPVideoHeaderVP8,
                                          RTPVideoHeaderVP9,
                                          RTPVideoHeaderH264,
                                          RTPVideoHeaderLegacyGeneric>;

struct RTPVideoHeader {
  struct GenericDescriptorInfo {
    GenericDescriptorInfo();
    GenericDescriptorInfo(const GenericDescriptorInfo& other);
    ~GenericDescriptorInfo();

    int64_t frame_id = 0;
    int spatial_index = 0;
    int temporal_index = 0;
    ::std::vector<DecodeTargetIndication> decode_target_indications;
    ::std::vector<int64_t> dependencies;
    ::std::vector<int> chain_diffs;
    ::std::bitset<32> active_decode_targets = ~uint32_t{0};
  };

  RTPVideoHeader();
  RTPVideoHeader(const RTPVideoHeader& other);
  ~RTPVideoHeader();

  ::std::optional<GenericDescriptorInfo> generic;

  VideoFrameType frame_type = VideoFrameType::kEmptyFrame;
  uint16_t width = 0;
  uint16_t height = 0;
  VideoRotation rotation = kVideoRotation_0;
  VideoContentType content_type = VideoContentType::UNSPECIFIED;
  bool is_first_packet_in_frame = false;
  bool is_last_packet_in_frame = false;
  bool is_last_frame_in_picture = true;
  uint8_t simulcastIdx = 0;
  VideoCodecType codec = VideoCodecType::kVideoCodecGeneric;

  // Color space information if available.
  ::std::optional<ColorSpace> color_space;

  ::std::optional<VideoPlayoutDelay> playout_delay;
  VideoSendTiming video_timing;
  // This field is meant for media quality testing purpose only. When enabled it
  // carries the video frame id field from the sender to the receiver.
  ::std::optional<uint16_t> video_frame_tracking_id;
  RTPVideoTypeHeader video_type_header;

  // When provided, is sent as is as an RTP header extension according to
  // http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time.
  // Otherwise, it is derived from other relevant information.
  ::std::optional<AbsoluteCaptureTime> absolute_capture_time;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_HEADER_H_
