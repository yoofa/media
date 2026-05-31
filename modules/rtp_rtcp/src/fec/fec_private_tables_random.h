/*
 * fec_private_tables_random.h - FEC random packet masks
 *
 * Copyright (c) 2024 The WebRTC project authors
 * Copyright (c) 2024 Aspect Software
 *
 * This file is derived from WebRTC's fec_private_tables_random.h
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_FEC_PRIVATE_TABLES_RANDOM_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_FEC_PRIVATE_TABLES_RANDOM_H_

// This file contains a set of packets masks for the FEC code. The masks in
// this table are specifically designed to favor recovery to random loss.
// These packet masks are defined to protect up to maximum of 48 media packets.

#include <cstdint>

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace fec_private_tables {

extern const uint8_t kPacketMaskRandomTbl[];

}  // namespace fec_private_tables
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_FEC_PRIVATE_TABLES_RANDOM_H_
