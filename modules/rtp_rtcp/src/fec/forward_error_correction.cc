/*
 * forward_error_correction.cc - FEC encoder/decoder implementation
 *
 * Copyright (c) 2012 The WebRTC project authors
 * Copyright (c) 2024 Aspect Software
 *
 * This file is derived from WebRTC's forward_error_correction.cc
 */

#include "media/modules/rtp_rtcp/src/fec/forward_error_correction.h"
#include <memory>
#include <span>

#include <cstring>

#include <algorithm>
#include <utility>

#include "base/checks.h"
#include "base/logging.h"
#include "base/numerics/mod_ops.h"
#include "base/numerics/sequence_number_util.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/fec/forward_error_correction_internal.h"
#include "media/modules/rtp_rtcp/src/fec/ulpfec_header_reader_writer.h"
#include "media/modules/rtp_rtcp/src/util/byte_io.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

using ::ave::base::IsNewerSequenceNumber;

namespace {
// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
constexpr size_t kTransportOverhead = 28;

constexpr uint16_t kOldSequenceThreshold = 0x3fff;
}  // namespace

ForwardErrorCorrection::Packet::Packet() : data(0) {}
ForwardErrorCorrection::Packet::~Packet() = default;

template <typename S, typename T>
bool ForwardErrorCorrection::SortablePacket::LessThan::operator()(
    const S& first,
    const T& second) {
  AVE_DCHECK_EQ(first->ssrc, second->ssrc);
  return IsNewerSequenceNumber(second->seq_num, first->seq_num);
}

ForwardErrorCorrection::ReceivedPacket::ReceivedPacket() = default;
ForwardErrorCorrection::ReceivedPacket::~ReceivedPacket() = default;

ForwardErrorCorrection::RecoveredPacket::RecoveredPacket() = default;
ForwardErrorCorrection::RecoveredPacket::~RecoveredPacket() = default;

ForwardErrorCorrection::ProtectedPacket::ProtectedPacket() = default;
ForwardErrorCorrection::ProtectedPacket::~ProtectedPacket() = default;

ForwardErrorCorrection::ReceivedFecPacket::ReceivedFecPacket() = default;
ForwardErrorCorrection::ReceivedFecPacket::~ReceivedFecPacket() = default;

ForwardErrorCorrection::ForwardErrorCorrection(
    std::unique_ptr<FecHeaderReader> fec_header_reader,
    std::unique_ptr<FecHeaderWriter> fec_header_writer,
    uint32_t ssrc,
    uint32_t protected_media_ssrc)
    : ssrc_(ssrc),
      protected_media_ssrc_(protected_media_ssrc),
      fec_header_reader_(std::move(fec_header_reader)),
      fec_header_writer_(std::move(fec_header_writer)),
      generated_fec_packets_(fec_header_writer_->MaxFecPackets()),
      packet_mask_size_(0) {}

ForwardErrorCorrection::~ForwardErrorCorrection() = default;

std::unique_ptr<ForwardErrorCorrection> ForwardErrorCorrection::CreateUlpfec(
    uint32_t ssrc) {
  std::unique_ptr<FecHeaderReader> fec_header_reader(new UlpfecHeaderReader());
  std::unique_ptr<FecHeaderWriter> fec_header_writer(new UlpfecHeaderWriter());
  return std::unique_ptr<ForwardErrorCorrection>(new ForwardErrorCorrection(
      std::move(fec_header_reader), std::move(fec_header_writer), ssrc, ssrc));
}

std::unique_ptr<ForwardErrorCorrection> ForwardErrorCorrection::CreateFlexfec(
    uint32_t /*ssrc*/,
    uint32_t /*protected_media_ssrc*/) {
  // FlexFEC not implemented yet - would need flexfec_header_reader_writer
  return nullptr;
}

int32_t ForwardErrorCorrection::EncodeFec(const PacketList& media_packets,
                                          uint8_t protection_factor,
                                          int32_t num_important_packets,
                                          bool use_unequal_protection,
                                          FecMaskType fec_mask_type,
                                          std::list<Packet*>* fec_packets) {
  const size_t num_media_packets = media_packets.size();

  // Sanity check arguments.
  AVE_DCHECK_GT(num_media_packets, 0);
  AVE_DCHECK_GE(num_important_packets, 0);
  AVE_DCHECK_LE(static_cast<size_t>(num_important_packets), num_media_packets);
  AVE_DCHECK(fec_packets->empty());
  const size_t max_media_packets = fec_header_writer_->MaxMediaPackets();
  if (num_media_packets > max_media_packets) {
    AVE_LOG(LS_WARNING) << "Can't protect " << num_media_packets
                        << " media packets per frame. Max is "
                        << max_media_packets << ".";
    return -1;
  }

  // Error check the media packets.
  for (const auto& media_packet : media_packets) {
    AVE_DCHECK(media_packet);
    if (media_packet->data.size() < kRtpHeaderSize) {
      AVE_LOG(LS_WARNING) << "Media packet " << media_packet->data.size()
                          << " bytes is smaller than RTP header.";
      return -1;
    }
    // Ensure the FEC packets will fit in a typical MTU.
    if (media_packet->data.size() + MaxPacketOverhead() + kTransportOverhead >
        IP_PACKET_SIZE) {
      AVE_LOG(LS_WARNING) << "Media packet " << media_packet->data.size()
                          << " bytes with overhead is larger than "
                          << IP_PACKET_SIZE << " bytes.";
    }
  }

  // Prepare generated FEC packets.
  int32_t num_fec_packets = NumFecPackets(num_media_packets, protection_factor);
  if (num_fec_packets == 0) {
    return 0;
  }

  // -- Generate packet masks --
  internal::PacketMaskTable mask_table(fec_mask_type, num_media_packets);
  internal::GeneratePacketMasks(num_media_packets, num_fec_packets,
                                num_important_packets, use_unequal_protection,
                                &mask_table, packet_masks_);

  packet_mask_size_ = internal::PacketMaskSize(num_media_packets);

  // -- Generate FEC packets --
  // Clear old FEC packets
  for (auto& fec_packet : generated_fec_packets_) {
    fec_packet.data.Clear();
  }

  // Reset to start
  if (generated_fec_packets_.size() < static_cast<size_t>(num_fec_packets)) {
    generated_fec_packets_.resize(num_fec_packets);
  }

  // Insert zero columns in the packet mask
  int32_t num_mask_bits =
      InsertZerosInPacketMasks(media_packets, num_fec_packets);
  if (num_mask_bits < 0) {
    AVE_LOG(LS_INFO) << "Too large sequence number gap.";
    return -1;
  }

  packet_mask_size_ = internal::PacketMaskSize(num_mask_bits);

  GenerateFecPayloads(media_packets, num_fec_packets);

  uint16_t first_seq_num = ByteReader<uint16_t>::ReadBigEndian(
      media_packets.front()->data.data() + 2);
  FinalizeFecHeaders(num_fec_packets, protected_media_ssrc_, first_seq_num);

  // Build the FEC packets return list
  for (size_t i = 0; std::cmp_less(i, num_fec_packets); ++i) {
    fec_packets->push_back(&generated_fec_packets_[i]);
  }

  return 0;
}

int32_t ForwardErrorCorrection::InsertZerosInPacketMasks(
    const PacketList& media_packets,
    size_t num_fec_packets) {
  size_t num_media_packets = media_packets.size();
  if (num_media_packets <= 1) {
    return num_media_packets;
  }

  // Get the first and last sequence number
  uint16_t first_seq_num = ByteReader<uint16_t>::ReadBigEndian(
      media_packets.front()->data.data() + 2);
  uint16_t last_seq_num = ByteReader<uint16_t>::ReadBigEndian(
      media_packets.back()->data.data() + 2);

  auto seq_num_span = static_cast<uint16_t>(last_seq_num - first_seq_num + 1);

  // Check if we need to insert zeros
  if (seq_num_span == num_media_packets) {
    // No gaps
    return num_media_packets;
  }

  // Too many zeros would result in a large packet mask
  const size_t max_media_packets = fec_header_writer_->MaxMediaPackets();
  if (seq_num_span > max_media_packets) {
    AVE_LOG(LS_WARNING) << "Sequence number span " << seq_num_span
                        << " too large for FEC.";
    return -1;
  }

  // Calculate needed packet mask size
  size_t new_packet_mask_bytes = internal::PacketMaskSize(seq_num_span);

  // Copy to temporary mask first
  memcpy(tmp_packet_masks_, packet_masks_, num_fec_packets * packet_mask_size_);
  memset(packet_masks_, 0, num_fec_packets * new_packet_mask_bytes);

  // Iterate through the media packets and build the new mask
  auto it = media_packets.begin();
  int32_t old_bit_index = 0;
  int32_t new_bit_index = 0;

  uint16_t expected_seq_num = first_seq_num;
  while (it != media_packets.end()) {
    uint16_t seq_num =
        ByteReader<uint16_t>::ReadBigEndian((*it)->data.data() + 2);

    // Insert zeros for missing sequence numbers
    int32_t num_zeros = static_cast<uint16_t>(seq_num - expected_seq_num);
    if (num_zeros > 0) {
      internal::InsertZeroColumns(num_zeros, packet_masks_,
                                  new_packet_mask_bytes, num_fec_packets,
                                  new_bit_index);
      new_bit_index += num_zeros;
    }

    // Copy the actual bit
    internal::CopyColumn(packet_masks_, new_packet_mask_bytes,
                         tmp_packet_masks_, packet_mask_size_, num_fec_packets,
                         new_bit_index, old_bit_index);

    ++old_bit_index;
    ++new_bit_index;
    expected_seq_num = seq_num + 1;
    ++it;
  }

  return seq_num_span;
}

void ForwardErrorCorrection::GenerateFecPayloads(
    const PacketList& media_packets,
    size_t num_fec_packets) {
  for (size_t fec_index = 0; fec_index < num_fec_packets; ++fec_index) {
    Packet& fec_packet = generated_fec_packets_[fec_index];
    size_t media_packet_index = 0;

    for (const auto& media_packet : media_packets) {
      // Check if this media packet is protected by this FEC packet
      size_t row_start = fec_index * packet_mask_size_;
      size_t byte_index = row_start + (media_packet_index / 8);
      size_t bit_index = 7 - (media_packet_index % 8);

      if ((packet_masks_[byte_index] >> bit_index) & 1) {
        // This media packet is protected by this FEC packet
        if (fec_packet.data.empty()) {
          // First protected packet - copy data
          size_t fec_header_size =
              fec_header_writer_->FecHeaderSize(packet_mask_size_);
          fec_packet.data.SetSize(media_packet->data.size() + fec_header_size -
                                  kRtpHeaderSize);
          // Clear header area
          memset(fec_packet.data.MutableData(), 0, fec_header_size);
          // Copy payload
          memcpy(fec_packet.data.MutableData() + fec_header_size,
                 media_packet->data.data() + kRtpHeaderSize,
                 media_packet->data.size() - kRtpHeaderSize);
          // XOR the header (first 2 bytes of RTP)
          fec_packet.data.MutableData()[0] ^= media_packet->data.data()[0];
          fec_packet.data.MutableData()[1] ^= media_packet->data.data()[1];
          // Store length recovery (bytes 2-3 temporary location for ULPFEC)
          uint16_t length = media_packet->data.size() - kRtpHeaderSize;
          ByteWriter<uint16_t>::WriteBigEndian(
              fec_packet.data.MutableData() + 2, length);
          // XOR timestamp
          for (int32_t i = 0; i < 4; ++i) {
            fec_packet.data.MutableData()[4 + i] ^=
                media_packet->data.data()[4 + i];
          }
        } else {
          // XOR with existing FEC packet
          size_t fec_header_size =
              fec_header_writer_->FecHeaderSize(packet_mask_size_);
          size_t media_payload_size =
              media_packet->data.size() - kRtpHeaderSize;

          // Expand FEC packet if needed
          if (fec_packet.data.size() < media_payload_size + fec_header_size) {
            size_t old_size = fec_packet.data.size();
            fec_packet.data.SetSize(media_payload_size + fec_header_size);
            memset(fec_packet.data.MutableData() + old_size, 0,
                   fec_packet.data.size() - old_size);
          }

          // XOR header (first 2 bytes)
          fec_packet.data.MutableData()[0] ^= media_packet->data.data()[0];
          fec_packet.data.MutableData()[1] ^= media_packet->data.data()[1];

          // XOR length recovery
          uint16_t old_length =
              ByteReader<uint16_t>::ReadBigEndian(fec_packet.data.data() + 2);
          uint16_t new_length = media_packet->data.size() - kRtpHeaderSize;
          ByteWriter<uint16_t>::WriteBigEndian(
              fec_packet.data.MutableData() + 2, old_length ^ new_length);

          // XOR timestamp
          for (int32_t i = 0; i < 4; ++i) {
            fec_packet.data.MutableData()[4 + i] ^=
                media_packet->data.data()[4 + i];
          }

          // XOR payload
          for (size_t i = 0; i < media_payload_size; ++i) {
            fec_packet.data.MutableData()[fec_header_size + i] ^=
                media_packet->data.data()[kRtpHeaderSize + i];
          }
        }
      }
      ++media_packet_index;
    }
  }
}

void ForwardErrorCorrection::FinalizeFecHeaders(size_t num_fec_packets,
                                                uint32_t media_ssrc,
                                                uint16_t seq_num_base) {
  for (size_t fec_index = 0; fec_index < num_fec_packets; ++fec_index) {
    Packet& fec_packet = generated_fec_packets_[fec_index];

    // Get packet mask for this FEC packet
    size_t row_start = fec_index * packet_mask_size_;
    std::span<const uint8_t> packet_mask(packet_masks_ + row_start,
                                         packet_mask_size_);

    FecHeaderWriter::ProtectedStream stream;
    stream.ssrc = media_ssrc;
    stream.seq_num_base = seq_num_base;
    stream.packet_mask = packet_mask;

    std::span<const FecHeaderWriter::ProtectedStream> streams(&stream, 1);

    fec_header_writer_->FinalizeFecHeader(streams, fec_packet);
  }
}

ForwardErrorCorrection::DecodeFecResult ForwardErrorCorrection::DecodeFec(
    const ReceivedPacket& received_packet,
    RecoveredPacketList* recovered_packets) {
  DecodeFecResult result;

  InsertPacket(received_packet, recovered_packets);
  result.num_recovered_packets = AttemptRecovery(recovered_packets);

  DiscardOldRecoveredPackets(recovered_packets);

  return result;
}

void ForwardErrorCorrection::InsertPacket(
    const ReceivedPacket& received_packet,
    RecoveredPacketList* recovered_packets) {
  if (received_packet.is_fec) {
    InsertFecPacket(*recovered_packets, received_packet);
  } else {
    InsertMediaPacket(recovered_packets, received_packet);
  }
}

void ForwardErrorCorrection::InsertMediaPacket(
    RecoveredPacketList* recovered_packets,
    const ReceivedPacket& received_packet) {
  // Check for duplicates
  for (const auto& recovered_packet : *recovered_packets) {
    if (recovered_packet->seq_num == received_packet.seq_num &&
        recovered_packet->ssrc == received_packet.ssrc) {
      return;  // Duplicate
    }
  }

  auto recovered_packet = std::make_unique<RecoveredPacket>();
  recovered_packet->ssrc = received_packet.ssrc;
  recovered_packet->seq_num = received_packet.seq_num;
  recovered_packet->was_recovered = false;
  recovered_packet->returned = false;
  recovered_packet->pkt = received_packet.pkt;

  // Insert in sorted order
  auto it = recovered_packets->begin();
  while (it != recovered_packets->end() &&
         SortablePacket::LessThan()((*it), recovered_packet)) {
    ++it;
  }
  recovered_packets->insert(it, std::move(recovered_packet));

  UpdateCoveringFecPackets(*recovered_packets->back());
}

void ForwardErrorCorrection::UpdateCoveringFecPackets(
    const RecoveredPacket& packet) {
  // For each FEC packet, check if it covers this media packet
  for (auto& fec_packet : received_fec_packets_) {
    for (auto& stream : fec_packet->protected_streams) {
      if (stream.ssrc != packet.ssrc) {
        continue;
      }

      auto delta = static_cast<uint16_t>(packet.seq_num - stream.seq_num_base);
      if (delta < stream.packet_mask_size * 8) {
        // This media packet is covered by this FEC packet
        auto& protected_packets = fec_packet->protected_packets;
        for (auto& protected_packet : protected_packets) {
          if (protected_packet->seq_num == packet.seq_num) {
            protected_packet->pkt = packet.pkt;
            break;
          }
        }
      }
    }
  }
}

void ForwardErrorCorrection::InsertFecPacket(
    const RecoveredPacketList& recovered_packets,
    const ReceivedPacket& received_packet) {
  // Check for duplicates
  for (const auto& fec_packet : received_fec_packets_) {
    if (fec_packet->seq_num == received_packet.seq_num &&
        fec_packet->ssrc == received_packet.ssrc) {
      return;  // Duplicate
    }
  }

  auto fec_packet = std::make_unique<ReceivedFecPacket>();
  fec_packet->ssrc = received_packet.ssrc;
  fec_packet->seq_num = received_packet.seq_num;
  fec_packet->pkt = received_packet.pkt;

  // Parse FEC header
  if (!fec_header_reader_->ReadFecHeader(fec_packet.get())) {
    AVE_LOG(LS_WARNING) << "Failed to parse FEC header";
    return;
  }

  // Set up protected packet list based on packet mask
  for (const auto& stream : fec_packet->protected_streams) {
    const uint8_t* packet_mask =
        fec_packet->pkt->data.data() + stream.packet_mask_offset;

    for (size_t byte_index = 0; byte_index < stream.packet_mask_size;
         ++byte_index) {
      for (int32_t bit_index = 7; bit_index >= 0; --bit_index) {
        if ((packet_mask[byte_index] >> bit_index) & 1) {
          auto protected_packet = std::make_unique<ProtectedPacket>();
          protected_packet->ssrc = stream.ssrc;
          protected_packet->seq_num =
              stream.seq_num_base + byte_index * 8 + (7 - bit_index);

          // Check if we already have this packet recovered
          for (const auto& recovered : recovered_packets) {
            if (recovered->ssrc == protected_packet->ssrc &&
                recovered->seq_num == protected_packet->seq_num) {
              protected_packet->pkt = recovered->pkt;
              break;
            }
          }

          fec_packet->protected_packets.push_back(std::move(protected_packet));
        }
      }
    }
  }

  // Insert in sorted order
  auto it = received_fec_packets_.begin();
  while (it != received_fec_packets_.end() &&
         SortablePacket::LessThan()((*it), fec_packet)) {
    ++it;
  }
  received_fec_packets_.insert(it, std::move(fec_packet));
}

void ForwardErrorCorrection::AssignRecoveredPackets(
    const RecoveredPacketList& recovered_packets,
    ReceivedFecPacket* fec_packet) {
  for (auto& protected_packet : fec_packet->protected_packets) {
    for (const auto& recovered : recovered_packets) {
      if (recovered->ssrc == protected_packet->ssrc &&
          recovered->seq_num == protected_packet->seq_num) {
        protected_packet->pkt = recovered->pkt;
        break;
      }
    }
  }
}

size_t ForwardErrorCorrection::AttemptRecovery(
    RecoveredPacketList* recovered_packets) {
  size_t num_recovered = 0;
  bool recovered_something = true;

  while (recovered_something) {
    recovered_something = false;

    for (auto& fec_packet : received_fec_packets_) {
      int32_t missing_count = NumCoveredPacketsMissing(*fec_packet);

      if (missing_count == 1) {
        // Can recover one packet
        auto recovered_packet = std::make_unique<RecoveredPacket>();

        if (RecoverPacket(*fec_packet, recovered_packet.get())) {
          recovered_packet->was_recovered = true;
          recovered_packet->returned = false;

          // Find the missing packet's seq_num
          for (const auto& protected_packet : fec_packet->protected_packets) {
            if (!protected_packet->pkt) {
              recovered_packet->ssrc = protected_packet->ssrc;
              recovered_packet->seq_num = protected_packet->seq_num;
              protected_packet->pkt = recovered_packet->pkt;
              break;
            }
          }

          // Insert recovered packet
          auto it = recovered_packets->begin();
          while (it != recovered_packets->end() &&
                 SortablePacket::LessThan()((*it), recovered_packet)) {
            ++it;
          }
          recovered_packets->insert(it, std::move(recovered_packet));

          UpdateCoveringFecPackets(*recovered_packets->back());
          ++num_recovered;
          recovered_something = true;
        }
      }
    }
  }

  return num_recovered;
}

bool ForwardErrorCorrection::StartPacketRecovery(
    const ReceivedFecPacket& fec_packet,
    RecoveredPacket* recovered_packet) {
  recovered_packet->pkt = std::make_shared<Packet>();
  recovered_packet->pkt->data.SetSize(fec_packet.protection_length +
                                      kRtpHeaderSize);

  // Copy FEC packet payload as starting point
  size_t fec_header_size = fec_packet.fec_header_size;
  memcpy(recovered_packet->pkt->data.MutableData() + kRtpHeaderSize,
         fec_packet.pkt->data.data() + fec_header_size,
         fec_packet.protection_length);

  // Start header recovery with FEC packet header data
  recovered_packet->pkt->data.MutableData()[0] = fec_packet.pkt->data.data()[0];
  recovered_packet->pkt->data.MutableData()[1] = fec_packet.pkt->data.data()[1];

  // Timestamp
  memcpy(recovered_packet->pkt->data.MutableData() + 4,
         fec_packet.pkt->data.data() + 4, 4);

  return true;
}

void ForwardErrorCorrection::XorHeaders(const Packet& src, Packet* dst) {
  dst->data.MutableData()[0] ^= src.data.data()[0];
  dst->data.MutableData()[1] ^= src.data.data()[1];

  // Timestamp (bytes 4-7)
  for (int32_t i = 0; i < 4; ++i) {
    dst->data.MutableData()[4 + i] ^= src.data.data()[4 + i];
  }
}

void ForwardErrorCorrection::XorPayloads(const Packet& src,
                                         size_t payload_length,
                                         size_t dst_offset,
                                         Packet* dst) {
  for (size_t i = 0; i < payload_length; ++i) {
    dst->data.MutableData()[dst_offset + i] ^=
        src.data.data()[kRtpHeaderSize + i];
  }
}

bool ForwardErrorCorrection::FinishPacketRecovery(
    const ReceivedFecPacket& fec_packet,
    RecoveredPacket* recovered_packet) {
  // Set recovered packet version, padding, extension, and CSRC count
  // from the XORed header recovery
  uint8_t byte0 = recovered_packet->pkt->data.data()[0];

  // Set version to 2
  byte0 &= 0x3f;
  byte0 |= 0x80;

  recovered_packet->pkt->data.MutableData()[0] = byte0;

  // Get SSRC from protected stream
  if (!fec_packet.protected_streams.empty()) {
    ByteWriter<uint32_t>::WriteBigEndian(
        recovered_packet->pkt->data.MutableData() + 8,
        fec_packet.protected_streams[0].ssrc);
  }

  return true;
}

bool ForwardErrorCorrection::RecoverPacket(const ReceivedFecPacket& fec_packet,
                                           RecoveredPacket* recovered_packet) {
  if (!StartPacketRecovery(fec_packet, recovered_packet)) {
    return false;
  }

  // XOR all available protected packets
  for (const auto& protected_packet : fec_packet.protected_packets) {
    if (protected_packet->pkt) {
      XorHeaders(*protected_packet->pkt, recovered_packet->pkt.get());
      size_t payload_length =
          protected_packet->pkt->data.size() - kRtpHeaderSize;
      XorPayloads(*protected_packet->pkt, payload_length, kRtpHeaderSize,
                  recovered_packet->pkt.get());
    }
  }

  return FinishPacketRecovery(fec_packet, recovered_packet);
}

int32_t ForwardErrorCorrection::NumCoveredPacketsMissing(
    const ReceivedFecPacket& fec_packet) {
  int32_t missing = 0;
  for (const auto& protected_packet : fec_packet.protected_packets) {
    if (!protected_packet->pkt) {
      ++missing;
      if (missing > 1) {
        return 2;  // Return early if more than one missing
      }
    }
  }
  return missing;
}

void ForwardErrorCorrection::DiscardOldRecoveredPackets(
    RecoveredPacketList* recovered_packets) {
  // Keep only recent packets to limit memory usage
  const size_t kMaxRecoveredPackets = 192;
  while (recovered_packets->size() > kMaxRecoveredPackets) {
    recovered_packets->pop_front();
  }

  // Also discard old FEC packets
  const size_t kMaxFecPackets = 48;
  while (received_fec_packets_.size() > kMaxFecPackets) {
    received_fec_packets_.pop_front();
  }
}

bool ForwardErrorCorrection::IsOldFecPacket(
    const ReceivedFecPacket& fec_packet,
    const RecoveredPacketList* recovered_packets) {
  if (recovered_packets->empty()) {
    return false;
  }

  uint16_t newest_seq = recovered_packets->back()->seq_num;
  uint16_t fec_seq = fec_packet.seq_num;

  return static_cast<uint16_t>(newest_seq - fec_seq) > kOldSequenceThreshold;
}

int32_t ForwardErrorCorrection::NumFecPackets(int32_t num_media_packets,
                                              int32_t protection_factor) {
  // (num_media_packets * protection_factor + 127) / 255
  int32_t num_fec_packets = (num_media_packets * protection_factor + 127) >> 8;
  // Ensure at least one FEC packet if protection is requested
  if (num_fec_packets == 0 && protection_factor > 0) {
    num_fec_packets = 1;
  }
  return num_fec_packets;
}

size_t ForwardErrorCorrection::MaxPacketOverhead() const {
  return fec_header_writer_->MaxPacketOverhead();
}

void ForwardErrorCorrection::ResetState(
    RecoveredPacketList* recovered_packets) {
  recovered_packets->clear();
  received_fec_packets_.clear();
}

uint16_t ForwardErrorCorrection::ParseSequenceNumber(const uint8_t* packet) {
  return ByteReader<uint16_t>::ReadBigEndian(packet + 2);
}

uint32_t ForwardErrorCorrection::ParseSsrc(const uint8_t* packet) {
  return ByteReader<uint32_t>::ReadBigEndian(packet + 8);
}

// FecHeaderReader implementation
FecHeaderReader::FecHeaderReader(size_t max_media_packets,
                                 size_t max_fec_packets)
    : max_media_packets_(max_media_packets),
      max_fec_packets_(max_fec_packets) {}

FecHeaderReader::~FecHeaderReader() = default;

size_t FecHeaderReader::MaxMediaPackets() const {
  return max_media_packets_;
}

size_t FecHeaderReader::MaxFecPackets() const {
  return max_fec_packets_;
}

// FecHeaderWriter implementation
FecHeaderWriter::FecHeaderWriter(size_t max_media_packets,
                                 size_t max_fec_packets,
                                 size_t max_packet_overhead)
    : max_media_packets_(max_media_packets),
      max_fec_packets_(max_fec_packets),
      max_packet_overhead_(max_packet_overhead) {}

FecHeaderWriter::~FecHeaderWriter() = default;

size_t FecHeaderWriter::MaxMediaPackets() const {
  return max_media_packets_;
}

size_t FecHeaderWriter::MaxFecPackets() const {
  return max_fec_packets_;
}

size_t FecHeaderWriter::MaxPacketOverhead() const {
  return max_packet_overhead_;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
