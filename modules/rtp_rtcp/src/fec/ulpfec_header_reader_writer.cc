/*
 * ulpfec_header_reader_writer.cc - ULPFEC header reader/writer implementation
 *
 * Copyright (c) 2016 The WebRTC project authors
 * Copyright (c) 2024 Aspect Software
 *
 * This file is derived from WebRTC's ulpfec_header_reader_writer.cc
 */

#include "media/modules/rtp_rtcp/src/fec/ulpfec_header_reader_writer.h"
#include <span>

#include <cstring>

#include "base/checks.h"
#include "media/modules/rtp_rtcp/src/fec/forward_error_correction_internal.h"
#include "media/modules/rtp_rtcp/src/util/byte_io.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

namespace {

// Maximum number of media packets that can be protected in one batch.
constexpr size_t kMaxMediaPackets = 48;

// Maximum number of media packets tracked by FEC decoder.
constexpr size_t kMaxTrackedMediaPackets = 4 * kMaxMediaPackets;

// Maximum number of FEC packets stored inside ForwardErrorCorrection.
constexpr size_t kMaxFecPackets = kMaxMediaPackets;

// FEC Level 0 header size in bytes.
constexpr size_t kFecLevel0HeaderSize = 10;

// FEC Level 1 (ULP) header size in bytes (L bit is set).
constexpr size_t kFecLevel1HeaderSizeLBitSet = 2 + kUlpfecPacketMaskSizeLBitSet;

// FEC Level 1 (ULP) header size in bytes (L bit is cleared).
constexpr size_t kFecLevel1HeaderSizeLBitClear =
    2 + kUlpfecPacketMaskSizeLBitClear;

constexpr size_t kPacketMaskOffset = kFecLevel0HeaderSize + 2;

size_t UlpfecHeaderSize(size_t packet_mask_size) {
  AVE_DCHECK_LE(packet_mask_size, kUlpfecPacketMaskSizeLBitSet);
  if (packet_mask_size <= kUlpfecPacketMaskSizeLBitClear) {
    return kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitClear;
  }
  return kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitSet;
}

}  // namespace

UlpfecHeaderReader::UlpfecHeaderReader()
    : FecHeaderReader(kMaxTrackedMediaPackets, kMaxFecPackets) {}

UlpfecHeaderReader::~UlpfecHeaderReader() = default;

bool UlpfecHeaderReader::ReadFecHeader(
    ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const {
  uint8_t* data = fec_packet->pkt->data.MutableData();
  if (fec_packet->pkt->data.size() < kPacketMaskOffset) {
    return false;  // Truncated packet.
  }
  bool l_bit = (data[0] & 0x40) != 0u;
  size_t packet_mask_size =
      l_bit ? kUlpfecPacketMaskSizeLBitSet : kUlpfecPacketMaskSizeLBitClear;
  fec_packet->fec_header_size = UlpfecHeaderSize(packet_mask_size);
  uint16_t seq_num_base = ByteReader<uint16_t>::ReadBigEndian(&data[2]);

  ForwardErrorCorrection::ProtectedStream stream;
  stream.ssrc = fec_packet->ssrc;  // Due to RED.
  stream.seq_num_base = seq_num_base;
  stream.packet_mask_offset = kPacketMaskOffset;
  stream.packet_mask_size = packet_mask_size;
  fec_packet->protected_streams.push_back(stream);

  fec_packet->protection_length =
      ByteReader<uint16_t>::ReadBigEndian(&data[10]);

  // Store length recovery field in temporary location in header.
  // This makes the header "compatible" with the corresponding
  // FlexFEC location of the length recovery field.
  memcpy(&data[2], &data[8], 2);

  return true;
}

UlpfecHeaderWriter::UlpfecHeaderWriter()
    : FecHeaderWriter(kMaxMediaPackets,
                      kMaxFecPackets,
                      kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitSet) {}

UlpfecHeaderWriter::~UlpfecHeaderWriter() = default;

size_t UlpfecHeaderWriter::MinPacketMaskSize(const uint8_t* /* packet_mask */,
                                             size_t packet_mask_size) const {
  return packet_mask_size;
}

size_t UlpfecHeaderWriter::FecHeaderSize(size_t packet_mask_size) const {
  return UlpfecHeaderSize(packet_mask_size);
}

void UlpfecHeaderWriter::FinalizeFecHeader(
    std::span<const ProtectedStream> protected_streams,
    ForwardErrorCorrection::Packet& fec_packet) const {
  AVE_CHECK_EQ(protected_streams.size(), 1);
  uint16_t seq_num_base = protected_streams[0].seq_num_base;
  const uint8_t* packet_mask = protected_streams[0].packet_mask.data();
  size_t packet_mask_size = protected_streams[0].packet_mask.size();

  uint8_t* data = fec_packet.data.MutableData();
  // Set E bit to zero.
  data[0] &= 0x7f;
  // Set L bit based on packet mask size.
  bool l_bit = (packet_mask_size == kUlpfecPacketMaskSizeLBitSet);
  if (l_bit) {
    data[0] |= 0x40;  // Set the L bit.
  } else {
    AVE_DCHECK_EQ(packet_mask_size, kUlpfecPacketMaskSizeLBitClear);
    data[0] &= 0xbf;  // Clear the L bit.
  }
  // Copy length recovery field from temporary location.
  memcpy(&data[8], &data[2], 2);
  // Write sequence number base.
  ByteWriter<uint16_t>::WriteBigEndian(&data[2], seq_num_base);
  // Protection length is set to entire packet.
  const size_t fec_header_size = FecHeaderSize(packet_mask_size);
  ByteWriter<uint16_t>::WriteBigEndian(
      &data[10], fec_packet.data.size() - fec_header_size);
  // Copy the packet mask.
  memcpy(&data[12], packet_mask, packet_mask_size);
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
