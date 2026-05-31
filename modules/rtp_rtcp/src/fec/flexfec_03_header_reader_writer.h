/*
 * flexfec_03_header_reader_writer.h - FlexFEC 03 header reader/writer
 *
 * Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-video-engine by aspect.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FLEXFEC_03_HEADER_READER_WRITER_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FLEXFEC_03_HEADER_READER_WRITER_H_

#include <stddef.h>
#include <stdint.h>
#include <span>

#include "media/modules/rtp_rtcp/src/fec/forward_error_correction.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// FlexFEC header, minimum 20 bytes.
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |R|F|P|X|  CC   |M| PT recovery |        length recovery        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                          TS recovery                          |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |   SSRCCount   |                    reserved                   |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// 12 |                             SSRC_i                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |           SN base_i           |k|          Mask [0-14]        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 20 |k|                   Mask [15-45] (optional)                   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 24 |k|                                                             |
//    +-+                   Mask [46-108] (optional)                  |
// 28 |                                                               |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//    :                     ... next in SSRC_i ...                    :
//
//
// FlexFEC header in 'inflexible' mode (F = 1), 20 bytes.
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |0|1|P|X|  CC   |M| PT recovery |        length recovery        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                          TS recovery                          |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |   SSRCCount   |                    reserved                   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |                             SSRC_i                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |           SN base_i           |  M (columns)  |    N (rows)   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class Flexfec03HeaderReader : public FecHeaderReader {
 public:
  Flexfec03HeaderReader();
  ~Flexfec03HeaderReader() override;

  bool ReadFecHeader(
      ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const override;
};

class Flexfec03HeaderWriter : public FecHeaderWriter {
 public:
  Flexfec03HeaderWriter();
  ~Flexfec03HeaderWriter() override;

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

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_FLEXFEC_03_HEADER_READER_WRITER_H_
