/*
 * forward_error_correction_internal.h - FEC internal utilities
 *
 * Copyright (c) 2024 The WebRTC project authors
 * Copyright (c) 2024 Aspect Software
 *
 * This file is derived from WebRTC's forward_error_correction_internal.h
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_

#include <cstddef>
#include <cstdint>

#include <span>
#include "media/modules/include/module_fec_types.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

// Maximum number of media packets that can be protected
// by these packet masks.
constexpr size_t kUlpfecMaxMediaPackets = 48;

// Packet mask size in bytes (given L bit).
constexpr size_t kUlpfecPacketMaskSizeLBitClear = 2;
constexpr size_t kUlpfecPacketMaskSizeLBitSet = 6;

// Packet code mask maximum length. kFECPacketMaskMaxSize = MaxNumFECPackets *
// (kUlpfecMaxMediaPackets / 8), and MaxNumFECPackets is equal to maximum number
// of media packets (kUlpfecMaxMediaPackets)
constexpr size_t kFECPacketMaskMaxSize = 288;

// Convenience constants.
constexpr size_t kUlpfecMinPacketMaskSize = kUlpfecPacketMaskSizeLBitClear;
constexpr size_t kUlpfecMaxPacketMaskSize = kUlpfecPacketMaskSizeLBitSet;

namespace internal {

class PacketMaskTable {
 public:
  PacketMaskTable(FecMaskType fec_mask_type, int32_t num_media_packets);
  ~PacketMaskTable();

  std::span<const uint8_t> LookUp(int32_t num_media_packets,
                                  int32_t num_fec_packets);

 private:
  static const uint8_t* PickTable(FecMaskType fec_mask_type,
                                  int32_t num_media_packets);
  const uint8_t* table_;
  uint8_t fec_packet_mask_[kFECPacketMaskMaxSize]{};
};

std::span<const uint8_t> LookUpInFecTable(const uint8_t* table,
                                          int32_t media_packet_index,
                                          int32_t fec_index);

// Returns an array of packet masks. The mask of a single FEC packet
// corresponds to a number of mask bytes. The mask indicates which
// media packets should be protected by the FEC packet.

// \param[in]  num_media_packets       The number of media packets to protect.
//                                     [1, max_media_packets].
// \param[in]  num_fec_packets         The number of FEC packets which will
//                                     be generated. [1, num_media_packets].
// \param[in]  num_imp_packets         The number of important packets.
//                                     [0, num_media_packets].
//                                     num_imp_packets = 0 is the equal
//                                     protection scenario.
// \param[in]  use_unequal_protection  Enables unequal protection: allocates
//                                     more protection to the num_imp_packets.
// \param[in]  mask_table              An instance of the `PacketMaskTable`
//                                     class, which contains the type of FEC
//                                     packet mask used, and a pointer to the
//                                     corresponding packet masks.
// \param[out] packet_mask             A pointer to hold the packet mask array,
//                                     of size: num_fec_packets *
//                                     "number of mask bytes".
void GeneratePacketMasks(int32_t num_media_packets,
                         int32_t num_fec_packets,
                         int32_t num_imp_packets,
                         bool use_unequal_protection,
                         PacketMaskTable* mask_table,
                         uint8_t* packet_mask);

// Returns the required packet mask size, given the number of sequence numbers
// that will be covered.
size_t PacketMaskSize(size_t num_sequence_numbers);

// Inserts `num_zeros` zero columns into `new_mask` at position
// `new_bit_index`. If the current byte of `new_mask` can't fit all zeros, the
// byte will be filled with zeros from `new_bit_index`, but the next byte will
// be untouched.
void InsertZeroColumns(int32_t num_zeros,
                       uint8_t* new_mask,
                       int32_t new_mask_bytes,
                       int32_t num_fec_packets,
                       int32_t new_bit_index);

// Copies the left most bit column from the byte pointed to by
// `old_bit_index` in `old_mask` to the right most column of the byte pointed
// to by `new_bit_index` in `new_mask`. `old_mask_bytes` and `new_mask_bytes`
// represent the number of bytes used per row for each mask. `num_fec_packets`
// represent the number of rows of the masks.
// The copied bit is shifted out from `old_mask` and is shifted one step to
// the left in `new_mask`. `new_mask` will contain "xxxx xxn0" after this
// operation, where x are previously inserted bits and n is the new bit.
void CopyColumn(uint8_t* new_mask,
                int32_t new_mask_bytes,
                uint8_t* old_mask,
                int32_t old_mask_bytes,
                int32_t num_fec_packets,
                int32_t new_bit_index,
                int32_t old_bit_index);

}  // namespace internal
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_
