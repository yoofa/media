/*
 * flexfec_sender.h - FlexFEC sender
 *
 * Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-video-engine by aspect.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_SENDER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_SENDER_H_

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <span>
#include "base/bitrate_tracker.h"
#include "base/clock.h"
#include "base/random.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/fec/ulpfec_generator.h"
#include "media/modules/rtp_rtcp/src/fec/video_fec_generator.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_header_extension_size.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Forward declaration
struct RtpExtension;

// Note that this class is not thread safe, and thus requires external
// synchronization. Currently, this is done using the lock in PayloadRouter.

class FlexfecSender : public VideoFecGenerator {
 public:
  FlexfecSender(base::Clock* clock,
                int32_t payload_type,
                uint32_t ssrc,
                uint32_t protected_media_ssrc,
                std::string mid,
                const std::vector<RtpExtension>& rtp_header_extensions,
                std::span<const RtpExtensionSize> extension_sizes,
                const RtpState* rtp_state);
  ~FlexfecSender() override;

  FecType GetFecType() const override {
    return VideoFecGenerator::FecType::kFlexFec;
  }
  std::optional<uint32_t> FecSsrc() override { return ssrc_; }

  // Sets the FEC rate, max frames sent before FEC packets are sent,
  // and what type of generator matrices are used.
  void SetProtectionParameters(const FecProtectionParams& delta_params,
                               const FecProtectionParams& key_params) override;

  // Adds a media packet to the internal buffer. When enough media packets
  // have been added, the FEC packets are generated and stored internally.
  // These FEC packets are then obtained by calling GetFecPackets().
  void AddPacketAndGenerateFec(const RtpPacketToSend& packet) override;

  // Returns generated FlexFEC packets.
  std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() override;

  // Returns the overhead, per packet, for FlexFEC.
  size_t MaxPacketOverhead() const override;

  DataRate CurrentFecRate() const override;

  // Only called on the VideoSendStream queue, after operation has shut down.
  std::optional<RtpState> GetRtpState() override;

 private:
  // Utility.
  base::Clock* const clock_;
  base::Random random_;
  base::Timestamp last_generated_packet_ = base::Timestamp::MinusInfinity();

  // Config.
  const int32_t payload_type_;
  const uint32_t timestamp_offset_;
  const uint32_t ssrc_;
  const uint32_t protected_media_ssrc_;
  // MID value to send in the MID header extension.
  const std::string mid_;
  // Sequence number of next packet to generate.
  uint16_t seq_num_;

  // Implementation.
  UlpfecGenerator ulpfec_generator_;
  const RtpHeaderExtensionMap rtp_header_extension_map_;
  const size_t header_extensions_size_;

  mutable std::mutex mutex_;
  BitrateTracker fec_bitrate_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_INCLUDE_FLEXFEC_SENDER_H_
