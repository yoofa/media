/*
 * fec_private_tables_bursty.h - FEC bursty packet masks
 *
 * Copyright (c) 2024 The WebRTC project authors
 * Copyright (c) 2024 Aspect Software
 *
 * This file is derived from WebRTC's fec_private_tables_bursty.h
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_FEC_PRIVATE_TABLES_BURSTY_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_FEC_PRIVATE_TABLES_BURSTY_H_

// This file contains a set of packets masks for the FEC code. The masks in
// this table are specifically designed to favor recovery of bursty/consecutive
// loss network conditions. The tradeoff is worse recovery for random losses.
// These packet masks are currently defined to protect up to 12 media packets.

#include <cstdint>

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace fec_private_tables {

extern const uint8_t kPacketMaskBurstyTbl[];

}  // namespace fec_private_tables
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_FEC_PRIVATE_TABLES_BURSTY_H_
