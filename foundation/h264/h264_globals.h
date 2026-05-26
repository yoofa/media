/*
 * h264_globals.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_FOUNDATION_H264_H264_GLOBALS_H_
#define AVE_MEDIA_FOUNDATION_H264_H264_GLOBALS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/checks.h"

namespace ave {
namespace media {

// The packetization types that we support: single, aggregated, and fragmented.
enum H264PacketizationTypes {
  kH264SingleNalu,  // This packet contains a single NAL unit.
  kH264StapA,       // This packet contains STAP-A (single time
                    // aggregation) packets. If this packet has an
                    // associated NAL unit type, it'll be for the
                    // first such aggregated packet.
  kH264FuA,         // This packet contains a FU-A (fragmentation
                    // unit) packet, meaning it is a part of a frame
                    // that was too large to fit into a single packet.
};

// Packetization modes are defined in RFC 6184 section 6
// Due to the structure containing this being initialized with zeroes
// in some places, and mode 1 being default, mode 1 needs to have the value
// zero.
enum class H264PacketizationMode {
  NonInterleaved = 0,  // Mode 1 - STAP-A, FU-A is allowed
  SingleNalUnit        // Mode 0 - only single NALU allowed
};

inline std::string ToString(H264PacketizationMode mode) {
  if (mode == H264PacketizationMode::NonInterleaved) {
    return "NonInterleaved";
  }
  if (mode == H264PacketizationMode::SingleNalUnit) {
    return "SingleNalUnit";
  }
  AVE_NOTREACHED();
  return "";
}

struct NaluInfo {
  uint8_t type;
  int sps_id;
  int pps_id;

  friend bool operator==(const NaluInfo& lhs, const NaluInfo& rhs) {
    return lhs.type == rhs.type && lhs.sps_id == rhs.sps_id &&
           lhs.pps_id == rhs.pps_id;
  }

  friend bool operator!=(const NaluInfo& lhs, const NaluInfo& rhs) {
    return !(lhs == rhs);
  }
};

struct RTPVideoHeaderH264 {
  // The NAL unit type. If this is a header for a
  // fragmented packet, it's the NAL unit type of
  // the original data. If this is the header for an
  // aggregated packet, it's the NAL unit type of
  // the first NAL unit in the packet.
  uint8_t nalu_type;
  // The packetization type of this buffer - single, aggregated or fragmented.
  H264PacketizationTypes packetization_type;
  std::vector<NaluInfo> nalus;
  // The packetization mode of this transport. Packetization mode
  // determines which packetization types are allowed when packetizing.
  H264PacketizationMode packetization_mode;

  friend bool operator==(const RTPVideoHeaderH264& lhs,
                         const RTPVideoHeaderH264& rhs) {
    return lhs.nalu_type == rhs.nalu_type &&
           lhs.packetization_type == rhs.packetization_type &&
           lhs.nalus == rhs.nalus &&
           lhs.packetization_mode == rhs.packetization_mode;
  }

  friend bool operator!=(const RTPVideoHeaderH264& lhs,
                         const RTPVideoHeaderH264& rhs) {
    return !(lhs == rhs);
  }
};

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_FOUNDATION_H264_H264_GLOBALS_H_
