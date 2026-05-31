/*
 * flexfec_header_reader_writer.h - FlexFEC header reader/writer
 *
 * Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-video-engine by aspect.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FLEXFEC_HEADER_READER_WRITER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FLEXFEC_HEADER_READER_WRITER_H_

#include <stddef.h>
#include <stdint.h>
#include <span>

#include "media/modules/rtp_rtcp/src/fec/forward_error_correction.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// FlexFEC header in flexible mode (R=0, F=0), minimum 12 bytes.
// https://datatracker.ietf.org/doc/html/rfc8627#section-4.2.2.1
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |0|0|P|X|  CC   |M| PT recovery |        length recovery        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                          TS recovery                          |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |           SN base_i           |k|          Mask [0-14]        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |k|                   Mask [15-45] (optional)                   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |                     Mask [46-109] (optional)                  |
// 20 |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |   ... next SN base and Mask for CSRC_i in CSRC list ...       |
//

class FlexfecHeaderReader : public FecHeaderReader {
 public:
  FlexfecHeaderReader();
  ~FlexfecHeaderReader() override;

  bool ReadFecHeader(
      ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const override;
};

class FlexfecHeaderWriter : public FecHeaderWriter {
 public:
  FlexfecHeaderWriter();
  ~FlexfecHeaderWriter() override;

  size_t MinPacketMaskSize(const uint8_t* packet_mask,
                           size_t packet_mask_size) const override;

  size_t FecHeaderSize(size_t packet_mask_row_size) const override;

  void FinalizeFecHeader(
      std::span<const ProtectedStream> protected_streams,
      ForwardErrorCorrection::Packet& fec_packet) const override;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FLEXFEC_HEADER_READER_WRITER_H_
