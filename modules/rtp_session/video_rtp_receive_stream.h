/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_MODULES_RTP_SESSION_VIDEO_RTP_RECEIVE_STREAM_H_
#define MEDIA_MODULES_RTP_SESSION_VIDEO_RTP_RECEIVE_STREAM_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include "base/numerics/sequence_number_unwrapper.h"
#include "base/task_util/task_runner_factory.h"
#include "media/foundation/async_media_frame_source.h"
#include "media/modules/rtp_rtcp/api/rtp_packet_sink_interface.h"
#include "media/modules/rtp_session/rtp_timestamp_mapper.h"

namespace ave::media::rtp_session {

// Reorders RFC 6184 packets belonging to one RTP timestamp and emits complete
// Annex-B H.264 access units. The class deliberately stops at encoded frames;
// decoding and rendering remain application-selectable sinks.
class VideoRtpReceiveStream final : public AsyncMediaFrameSource,
                                    public rtp_rtcp::RtpPacketSinkInterface {
 public:
  struct Config {
    uint32_t remote_ssrc = 0;
    uint8_t payload_type = 102;
    uint32_t rtp_clock_rate_hz = 90000;
    size_t max_packets_per_frame = 512;
    base::TaskRunnerFactory* task_runner_factory = nullptr;
  };

  struct Stats {
    uint64_t packets_received = 0;
    uint64_t frames_delivered = 0;
    uint64_t incomplete_frames_dropped = 0;
    uint64_t invalid_packets_dropped = 0;
  };

  explicit VideoRtpReceiveStream(const Config& config);
  ~VideoRtpReceiveStream() override;

  void Start();
  void Stop();
  void OnRtpPacket(const rtp_rtcp::RtpPacketReceived& packet) override;
  Stats GetStats() const;

 private:
  struct PacketFragment {
    bool marker = false;
    bool key_frame = false;
    std::vector<uint8_t> payload;
  };

  struct PendingFrame {
    uint32_t rtp_timestamp = 0;
    base::Timestamp arrival_time = base::Timestamp::MinusInfinity();
    std::map<int64_t, PacketFragment> packets;
    std::optional<int64_t> first_sequence;
    std::optional<int64_t> marker_sequence;
  };

  static std::optional<PacketFragment> ParsePacket(
      const rtp_rtcp::RtpPacketReceived& packet);
  std::shared_ptr<MediaFrame> TryAssembleFrame();
  void ResetPendingFrame(uint32_t rtp_timestamp, base::Timestamp arrival_time);

  const Config config_;
  base::RtpSequenceNumberUnwrapper sequence_unwrapper_;
  RtpReceiveTimestampMapper timestamp_mapper_;
  std::optional<PendingFrame> pending_frame_;
  mutable std::mutex mutex_;
  std::atomic<bool> running_{false};
  Stats stats_;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_VIDEO_RTP_RECEIVE_STREAM_H_
