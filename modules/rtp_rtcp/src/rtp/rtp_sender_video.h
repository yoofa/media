/*
 * rtp_sender_video.h - RTP Video Sender
 *
 * Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 * Copyright (c) 2024 The aspect project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source tree.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <span>
#include "base/bitrate_tracker.h"
#include "base/clock.h"
#include "base/frequency_tracker.h"
#include "base/one_time_event.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/foundation/color_space.h"
#include "media/foundation/video_codec_type.h"
#include "media/foundation/video_frame_type.h"
#include "media/foundation/video_layers_allocation.h"
#include "media/foundation/video_rotation.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/fec/video_fec_generator.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_sender.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"
#include "media/modules/rtp_rtcp/src/util/absolute_capture_time_sender.h"
#include "media/modules/rtp_rtcp/src/util/active_decode_targets_helper.h"
#include "media/modules/rtp_rtcp/src/util/dependency_descriptor.h"
#include "media/modules/rtp_rtcp/src/util/rtp_rtcp_config.h"
#include "media/modules/rtp_rtcp/src/video/rtp_format.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace base = ::ave::base;
using BitrateTracker = ::ave::base::BitrateTracker;
using FrequencyTracker = ::ave::base::FrequencyTracker;
using OneTimeEvent = ::ave::base::OneTimeEvent;
using Clock = ::ave::base::Clock;
using Frequency = ::ave::base::Frequency;

// All types are now in ave::media::rtp_rtcp namespace

// kConditionallyRetransmitHigherLayers allows retransmission of video frames
// in higher layers if either the last frame in that layer was too far back in
// time, or if we estimate that a new frame will be available in a lower layer
// in a shorter time than it would take to request and receive a retransmission.
enum RetransmissionMode : uint8_t {
  kRetransmitOff = 0x0,
  kRetransmitBaseLayer = 0x2,
  kRetransmitHigherLayers = 0x4,
  kRetransmitAllLayers = 0x6,
  kConditionallyRetransmitHigherLayers = 0x8
};

// Interface for video RTP frame sender.
class RTPVideoFrameSenderInterface {
 public:
  virtual ~RTPVideoFrameSenderInterface() = default;

  virtual bool SendVideo(int payload_type,
                         std::optional<VideoCodecType> codec_type,
                         uint32_t rtp_timestamp,
                         Timestamp capture_time,
                         std::span<const uint8_t> payload,
                         size_t encoder_output_size,
                         RTPVideoHeader video_header,
                         TimeDelta expected_retransmission_time,
                         std::vector<uint32_t> csrcs) = 0;

  virtual void SetVideoStructureAfterTransformation(
      const FrameDependencyStructure* video_structure) = 0;

  virtual void SetVideoLayersAllocationAfterTransformation(
      VideoLayersAllocation allocation) = 0;
};

class RTPSenderVideo : public RTPVideoFrameSenderInterface {
 public:
  static constexpr TimeDelta kTLRateWindowSize = TimeDelta::Millis(2'500);

  struct Config {
    Config() = default;
    Config(const Config&) = delete;
    Config(Config&&) = default;

    // All members of this struct are expected to outlive the RTPSenderVideo
    // object they are passed to.
    base::Clock* clock = nullptr;
    RTPSender* rtp_sender = nullptr;
    // Some FEC data is duplicated here in preparation of moving FEC to
    // the egress stage.
    std::optional<VideoFecGenerator::FecType> fec_type;
    size_t fec_overhead_bytes = 0;  // Per packet max FEC overhead.
    bool enable_retransmit_all_layers = false;
    std::optional<int> red_payload_type;
  };

  explicit RTPSenderVideo(const Config& config);

  ~RTPSenderVideo() override;

  // `capture_time` and `clock::CurrentTime` should be using the same epoch.
  // `expected_retransmission_time.IsFinite()` -> retransmission allowed.
  // `encoder_output_size` is the size of the video frame as it came out of the
  // video encoder, excluding any additional overhead.
  // Calls to this method are assumed to be externally serialized.
  bool SendVideo(int payload_type,
                 std::optional<VideoCodecType> codec_type,
                 uint32_t rtp_timestamp,
                 Timestamp capture_time,
                 std::span<const uint8_t> payload,
                 size_t encoder_output_size,
                 RTPVideoHeader video_header,
                 TimeDelta expected_retransmission_time,
                 std::vector<uint32_t> csrcs) override;

  // Configures video structures produced by encoder to send using the
  // dependency descriptor rtp header extension. Next call to SendVideo should
  // have video_header.frame_type == kVideoFrameKey.
  // All calls to SendVideo after this call must use video_header compatible
  // with the video_structure.
  void SetVideoStructure(const FrameDependencyStructure* video_structure);
  // Should only be used by a RTPSenderVideoFrameTransformerDelegate and exists
  // to ensure correct synchronization.
  void SetVideoStructureAfterTransformation(
      const FrameDependencyStructure* video_structure) override;

  // Sets current active VideoLayersAllocation. The allocation will be sent
  // using the rtp video layers allocation extension. The allocation will be
  // sent in full on every key frame. The allocation will be sent once on a
  // none discardable delta frame per call to this method and will not contain
  // resolution and frame rate.
  void SetVideoLayersAllocation(VideoLayersAllocation allocation);
  // Should only be used by a RTPSenderVideoFrameTransformerDelegate and exists
  // to ensure correct synchronization.
  void SetVideoLayersAllocationAfterTransformation(
      VideoLayersAllocation allocation) override;

  // Returns the current post encode overhead rate, in bps. Note that this is
  // the payload overhead, eg the VP8 payload headers and any other added
  // metadata added by transforms. It does not include the RTP headers or
  // extensions.
  DataRate PostEncodeOverhead() const;

  // 'retransmission_mode' is either a value of enum RetransmissionMode, or
  // computed with bitwise operators on values of enum RetransmissionMode.
  void SetRetransmissionSetting(int32_t retransmission_settings);

 protected:
  static uint8_t GetTemporalId(const RTPVideoHeader& header);
  bool AllowRetransmission(uint8_t temporal_id,
                           int32_t retransmission_settings,
                           TimeDelta expected_retransmission_time);

 private:
  struct TemporalLayerStats {
    FrequencyTracker frame_rate{kTLRateWindowSize};
    Timestamp last_frame_time = Timestamp::Zero();
  };

  enum class SendVideoLayersAllocation {
    kSendWithResolution,
    kSendWithoutResolution,
    kDontSend
  };

  void SetVideoStructureInternal(
      const FrameDependencyStructure* video_structure);
  void SetVideoLayersAllocationInternal(VideoLayersAllocation allocation);

  void AddRtpHeaderExtensions(const RTPVideoHeader& video_header,
                              bool first_packet,
                              bool last_packet,
                              RtpPacketToSend* packet) const;

  size_t FecPacketOverhead() const;

  void LogAndSendToNetwork(
      std::vector<std::unique_ptr<RtpPacketToSend>> packets,
      size_t encoder_output_size);

  bool red_enabled() const { return red_payload_type_.has_value(); }

  bool UpdateConditionalRetransmit(uint8_t temporal_id,
                                   TimeDelta expected_retransmission_time);

  void MaybeUpdateCurrentPlayoutDelay(const RTPVideoHeader& header);

  RTPSender* const rtp_sender_;
  base::Clock* const clock_;

  // These members should only be accessed from within SendVideo() to avoid
  // potential race conditions.
  int32_t retransmission_settings_;
  VideoRotation last_rotation_;
  std::optional<ColorSpace> last_color_space_;
  bool transmit_color_space_next_frame_;
  std::unique_ptr<FrameDependencyStructure> video_structure_;
  std::optional<VideoLayersAllocation> allocation_;
  // Flag indicating if we should send `allocation_`.
  SendVideoLayersAllocation send_allocation_;
  std::optional<VideoLayersAllocation> last_full_sent_allocation_;

  // Current target playout delay.
  std::optional<VideoPlayoutDelay> current_playout_delay_;
  // Flag indicating if we need to send `current_playout_delay_` in order
  // to guarantee it gets delivered.
  bool playout_delay_pending_;
  // Forced playout delay override (if set).
  std::optional<VideoPlayoutDelay> forced_playout_delay_;

  // Should never be held when calling out of this class.
  mutable std::mutex mutex_;

  const std::optional<int> red_payload_type_;
  std::optional<VideoFecGenerator::FecType> fec_type_;
  const size_t fec_overhead_bytes_;  // Per packet max FEC overhead.

  mutable std::mutex stats_mutex_;
  BitrateTracker post_encode_overhead_bitrate_;

  std::map<int, TemporalLayerStats> frame_stats_by_temporal_layer_;

  OneTimeEvent first_frame_sent_;

  AbsoluteCaptureTimeSender absolute_capture_time_sender_;
  // Tracks updates to the active decode targets and decides when active decode
  // targets bitmask should be attached to the dependency descriptor.
  ActiveDecodeTargetsHelper active_decode_targets_tracker_;

  // Whether to do two-pass packetization for AV1 which leads to a set of
  // packets with more even size distribution.
  const bool enable_av1_even_split_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
