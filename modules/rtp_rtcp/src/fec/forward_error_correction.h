/*
 * forward_error_correction.h - FEC encoder/decoder
 *
 * Copyright (c) 2012 The WebRTC project authors
 * Copyright (c) 2024 Aspect Software
 *
 * This file is derived from WebRTC's forward_error_correction.h
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include <span>
#include "base/copy_on_write_buffer.h"

#include "media/modules/include/module_fec_types.h"
#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/fec/forward_error_correction_internal.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class FecHeaderReader;
class FecHeaderWriter;

// Performs codec-independent forward error correction (FEC), based on RFC 5109.
// Option exists to enable unequal protection (UEP) across packets.
class ForwardErrorCorrection {
 public:
  // Packet class with reference counting
  class Packet {
   public:
    Packet();
    virtual ~Packet();

    // Add a reference.
    virtual int32_t AddRef();

    // Release a reference. Will delete the object if the reference count
    // reaches zero.
    virtual int32_t Release();

    base::CopyOnWriteBuffer data;  // Packet data.

   private:
    // Counts the number of references to a packet.
  };

  // Sortable packet base class
  class SortablePacket {
   public:
    // Functor which returns true if the sequence number of `first`
    // is < the sequence number of `second`.
    struct LessThan {
      template <typename S, typename T>
      bool operator()(const S& first, const T& second);
    };

    uint32_t ssrc;
    uint16_t seq_num;
  };

  // Used for the input to DecodeFec().
  class ReceivedPacket : public SortablePacket {
   public:
    ReceivedPacket();
    ~ReceivedPacket();

    bool is_fec;  // Set to true if this is an FEC packet.
    bool is_recovered;
    RtpHeaderExtensionMap extensions;
    std::shared_ptr<Packet> pkt;  // Pointer to the packet storage.
  };

  // The recovered list parameter of DecodeFec()
  class RecoveredPacket : public SortablePacket {
   public:
    RecoveredPacket();
    ~RecoveredPacket();

    bool was_recovered;  // Will be true if this packet was recovered by FEC.
    bool returned;       // True when the packet already has been returned.
    std::shared_ptr<Packet> pkt;  // Pointer to the packet storage.
  };

  // Used to link media packets to their protecting FEC packets.
  class ProtectedPacket : public SortablePacket {
   public:
    ProtectedPacket();
    ~ProtectedPacket();

    std::shared_ptr<ForwardErrorCorrection::Packet> pkt;
  };

  using ProtectedPacketList = std::list<std::unique_ptr<ProtectedPacket>>;

  struct ProtectedStream {
    uint32_t ssrc = 0;
    uint16_t seq_num_base = 0;
    size_t packet_mask_offset = 0;  // Relative start of FEC header.
    size_t packet_mask_size = 0;
  };

  // Used for internal storage of received FEC packets
  class ReceivedFecPacket : public SortablePacket {
   public:
    static constexpr size_t kInlinedSsrcsVectorSize = 4;

    ReceivedFecPacket();
    ~ReceivedFecPacket();

    // List of media packets that this FEC packet protects.
    ProtectedPacketList protected_packets;
    // RTP header fields.
    uint32_t ssrc;
    // FEC header fields.
    size_t fec_header_size;
    std::vector<ProtectedStream> protected_streams;
    size_t protection_length;
    // Raw data.
    std::shared_ptr<ForwardErrorCorrection::Packet> pkt;
  };

  using PacketList = std::list<std::unique_ptr<Packet>>;
  using RecoveredPacketList = std::list<std::unique_ptr<RecoveredPacket>>;
  using ReceivedFecPacketList = std::list<std::unique_ptr<ReceivedFecPacket>>;

  ~ForwardErrorCorrection();

  // Creates a ForwardErrorCorrection tailored for a specific FEC scheme.
  static std::unique_ptr<ForwardErrorCorrection> CreateUlpfec(uint32_t ssrc);
  static std::unique_ptr<ForwardErrorCorrection> CreateFlexfec(
      uint32_t ssrc,
      uint32_t protected_media_ssrc);

  // Generates a list of FEC packets from supplied media packets.
  int EncodeFec(const PacketList& media_packets,
                uint8_t protection_factor,
                int num_important_packets,
                bool use_unequal_protection,
                FecMaskType fec_mask_type,
                std::list<Packet*>* fec_packets);

  // Decodes a list of received media and FEC packets.
  struct DecodeFecResult {
    size_t num_recovered_packets = 0;
  };

  DecodeFecResult DecodeFec(const ReceivedPacket& received_packet,
                            RecoveredPacketList* recovered_packets);

  // Get the number of generated FEC packets.
  static int NumFecPackets(int num_media_packets, int protection_factor);

  // Gets the maximum size of the FEC headers in bytes.
  size_t MaxPacketOverhead() const;

  // Reset internal states and clear `recovered_packets`.
  void ResetState(RecoveredPacketList* recovered_packets);

  // Parse helpers.
  static uint16_t ParseSequenceNumber(const uint8_t* packet);
  static uint32_t ParseSsrc(const uint8_t* packet);

 protected:
  ForwardErrorCorrection(std::unique_ptr<FecHeaderReader> fec_header_reader,
                         std::unique_ptr<FecHeaderWriter> fec_header_writer,
                         uint32_t ssrc,
                         uint32_t protected_media_ssrc);

 private:
  int InsertZerosInPacketMasks(const PacketList& media_packets,
                               size_t num_fec_packets);

  void GenerateFecPayloads(const PacketList& media_packets,
                           size_t num_fec_packets);

  void FinalizeFecHeaders(size_t num_fec_packets,
                          uint32_t media_ssrc,
                          uint16_t seq_num_base);

  void InsertPacket(const ReceivedPacket& received_packet,
                    RecoveredPacketList* recovered_packets);

  void InsertMediaPacket(RecoveredPacketList* recovered_packets,
                         const ReceivedPacket& received_packet);

  void UpdateCoveringFecPackets(const RecoveredPacket& packet);

  void InsertFecPacket(const RecoveredPacketList& recovered_packets,
                       const ReceivedPacket& received_packet);

  static void AssignRecoveredPackets(
      const RecoveredPacketList& recovered_packets,
      ReceivedFecPacket* fec_packet);

  size_t AttemptRecovery(RecoveredPacketList* recovered_packets);

  static bool StartPacketRecovery(const ReceivedFecPacket& fec_packet,
                                  RecoveredPacket* recovered_packet);

  static void XorHeaders(const Packet& src, Packet* dst);

  static void XorPayloads(const Packet& src,
                          size_t payload_length,
                          size_t dst_offset,
                          Packet* dst);

  static bool FinishPacketRecovery(const ReceivedFecPacket& fec_packet,
                                   RecoveredPacket* recovered_packet);

  static bool RecoverPacket(const ReceivedFecPacket& fec_packet,
                            RecoveredPacket* recovered_packet);

  static int NumCoveredPacketsMissing(const ReceivedFecPacket& fec_packet);

  void DiscardOldRecoveredPackets(RecoveredPacketList* recovered_packets);

  bool IsOldFecPacket(const ReceivedFecPacket& fec_packet,
                      const RecoveredPacketList* recovered_packets);

  [[maybe_unused]] const uint32_t ssrc_;
  const uint32_t protected_media_ssrc_;

  std::unique_ptr<FecHeaderReader> fec_header_reader_;
  std::unique_ptr<FecHeaderWriter> fec_header_writer_;

  std::vector<Packet> generated_fec_packets_;
  ReceivedFecPacketList received_fec_packets_;

  uint8_t packet_masks_[kUlpfecMaxMediaPackets * kUlpfecMaxPacketMaskSize];
  uint8_t tmp_packet_masks_[kUlpfecMaxMediaPackets * kUlpfecMaxPacketMaskSize];
  size_t packet_mask_size_;
};

// FEC Header reader base class
class FecHeaderReader {
 public:
  virtual ~FecHeaderReader();

  size_t MaxMediaPackets() const;
  size_t MaxFecPackets() const;

  virtual bool ReadFecHeader(
      ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const = 0;

 protected:
  FecHeaderReader(size_t max_media_packets, size_t max_fec_packets);

  const size_t max_media_packets_;
  const size_t max_fec_packets_;
};

// FEC Header writer base class
class FecHeaderWriter {
 public:
  struct ProtectedStream {
    uint32_t ssrc = 0;
    uint16_t seq_num_base = 0;
    std::span<const uint8_t> packet_mask;
  };

  virtual ~FecHeaderWriter();

  size_t MaxMediaPackets() const;
  size_t MaxFecPackets() const;
  size_t MaxPacketOverhead() const;

  virtual size_t MinPacketMaskSize(const uint8_t* packet_mask,
                                   size_t packet_mask_size) const = 0;

  virtual size_t FecHeaderSize(size_t packet_mask_size) const = 0;

  virtual void FinalizeFecHeader(
      std::span<const ProtectedStream> protected_streams,
      ForwardErrorCorrection::Packet& fec_packet) const = 0;

 protected:
  FecHeaderWriter(size_t max_media_packets,
                  size_t max_fec_packets,
                  size_t max_packet_overhead);

  const size_t max_media_packets_;
  const size_t max_fec_packets_;
  const size_t max_packet_overhead_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
