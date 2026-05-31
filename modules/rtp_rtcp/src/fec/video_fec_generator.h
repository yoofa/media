/*
 * video_fec_generator.h - Video FEC generator interface
 *
 * Copyright (c) 2025 The AveStart project authors. All Rights Reserved.
 *
 * Ported from WebRTC project.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_FEC_GENERATOR_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_FEC_GENERATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/units/data_rate.h"
#include "media/modules/include/module_fec_types.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class VideoFecGenerator {
 public:
  VideoFecGenerator() = default;
  virtual ~VideoFecGenerator() = default;

  enum class FecType { kFlexFec, kUlpFec };
  virtual FecType GetFecType() const = 0;

  // Returns the SSRC used for FEC packets (i.e. FlexFec SSRC).
  virtual std::optional<uint32_t> FecSsrc() = 0;

  // Returns the overhead, in bytes per packet, for FEC (and possibly RED).
  virtual size_t MaxPacketOverhead() const = 0;

  // Current rate of FEC packets generated, including all RTP-level headers.
  virtual DataRate CurrentFecRate() const = 0;

  // Set FEC rates, max frames before FEC is sent, and type of FEC masks.
  virtual void SetProtectionParameters(
      const FecProtectionParams& delta_params,
      const FecProtectionParams& key_params) = 0;

  // Called on new media packet to be protected. The generator may choose
  // to generate FEC packets at this time, if so they will be stored in an
  // internal buffer.
  virtual void AddPacketAndGenerateFec(const RtpPacketToSend& packet) = 0;

  // Get (and remove) and FEC packets pending in the generator. These packets
  // will lack sequence numbers, that needs to be set externally.
  virtual std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() = 0;

  // Only called on the VideoSendStream queue, after operation has shut down,
  // and only populated if there is an RtpState (e.g. FlexFec).
  virtual std::optional<RtpState> GetRtpState() = 0;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_VIDEO_FEC_GENERATOR_H_
