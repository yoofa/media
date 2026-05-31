/*
 * ulpfec_generator.h
 * Copyright (C) 2024 iWhaleshark <iwhalesharki@gmail.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_
#define MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "base/bitrate_tracker.h"
#include "base/clock.h"
#include "media/modules/include/module_fec_types.h"
#include "media/modules/rtp_rtcp/src/fec/forward_error_correction.h"
#include "media/modules/rtp_rtcp/src/fec/video_fec_generator.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

using BitrateTracker = ::ave::base::BitrateTracker;
using DataRate = ::ave::base::DataRate;

class FlexfecSender;

class UlpfecGenerator : public VideoFecGenerator {
  friend class FlexfecSender;

 public:
  UlpfecGenerator(base::Clock* clock,
                  int red_payload_type,
                  int ulpfec_payload_type);
  ~UlpfecGenerator();

  FecType GetFecType() const override {
    return VideoFecGenerator::FecType::kUlpFec;
  }
  std::optional<uint32_t> FecSsrc() override { return std::nullopt; }

  void SetProtectionParameters(const FecProtectionParams& delta_params,
                               const FecProtectionParams& key_params) override;

  // Adds a media packet to the internal buffer. When enough media packets
  // have been added, the FEC packets are generated and stored internally.
  // These FEC packets are then obtained by calling GetFecPacketsAsRed().
  void AddPacketAndGenerateFec(const RtpPacketToSend& packet) override;

  // Returns the overhead, per packet, for FEC (and possibly RED).
  size_t MaxPacketOverhead() const override;

  std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() override;

  // Current rate of FEC packets generated, including all RTP-level headers.
  DataRate CurrentFecRate() const override;

  std::optional<RtpState> GetRtpState() override { return std::nullopt; }

  // Currently used protection params.
  const FecProtectionParams& CurrentParams() const;

 private:
  struct Params {
    Params();
    Params(FecProtectionParams delta_params,
           FecProtectionParams keyframe_params);

    FecProtectionParams delta_params;
    FecProtectionParams keyframe_params;
  };

  UlpfecGenerator(base::Clock* clock,
                  std::unique_ptr<ForwardErrorCorrection> fec);

  // Overhead is defined as relative to the number of media packets, and not
  // relative to total number of packets. This definition is inherited from the
  // protection factor produced by video_coding module and how the FEC
  // generation is implemented.
  int Overhead() const;

  // Returns true if the excess overhead (actual - target) for the FEC is below
  // the amount `kMaxExcessOverhead`. This effects the lower protection level
  // cases and low number of media packets/frame. The target overhead is given
  // by `params_.fec_rate`, and is only achievable in the limit of large number
  // of media packets.
  bool ExcessOverheadBelowMax() const;

  // Returns true if the number of added media packets is at least
  // `min_num_media_packets_`. This condition tries to capture the effect
  // that, for the same amount of protection/overhead, longer codes
  // (e.g. (2k,2m) vs (k,m)) are generally more effective at recovering losses.
  bool MinimumMediaPacketsReached() const;

  void ResetState();

  base::Clock* const clock_;
  const int red_payload_type_;
  const int ulpfec_payload_type_;

  const std::unique_ptr<ForwardErrorCorrection> fec_;
  ForwardErrorCorrection::PacketList media_packets_;
  std::optional<RtpPacketToSend> last_media_packet_;
  std::list<ForwardErrorCorrection::Packet*> generated_fec_packets_;
  int num_protected_frames_;
  int min_num_media_packets_;
  Params current_params_;
  bool media_contains_keyframe_;

  mutable std::mutex mutex_;
  std::optional<Params> pending_params_;
  BitrateTracker fec_bitrate_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_
