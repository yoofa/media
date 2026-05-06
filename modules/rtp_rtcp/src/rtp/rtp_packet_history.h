/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Ported to AVE from WebRTC.
 */

#ifndef AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_
#define AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <span>
#include "base/clock.h"
#include "base/function_view.h"
#include "base/units/time_delta.h"
#include "base/units/timestamp.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_packet_to_send.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

class RtpPacketHistory {
 public:
  enum class StorageMode {
    kDisabled,     // Don't store any packets.
    kStoreAndCull  // Store up to `number_to_store` packets, but try to remove
                   // packets as they time out or as signaled as received.
  };

  enum class PaddingMode {
    kDefault,  // Last packet stored in the history that has not yet been
               // culled.
    kRecentLargePacket  // Use the most recent large packet. Packet is kept for
                        // padding even after it has been culled from history.
  };

  // Maximum number of packets we ever allow in the history.
  static constexpr size_t kMaxCapacity = 9600;
  // Maximum number of entries in prioritized queue of padding packets.
  static constexpr size_t kMaxPaddingHistory = 63;
  // Don't remove packets within max(1 second, 3x RTT).
  static constexpr base::TimeDelta kMinPacketDuration =
      base::TimeDelta::Seconds(1);
  static constexpr int32_t kMinPacketDurationRtt = 3;
  // With kStoreAndCull, always remove packets after 3x max(1000ms, 3x rtt).
  static constexpr int32_t kPacketCullingDelayFactor = 3;

  RtpPacketHistory(base::Clock* clock, PaddingMode padding_mode);

  RtpPacketHistory() = delete;
  RtpPacketHistory(const RtpPacketHistory&) = delete;
  RtpPacketHistory& operator=(const RtpPacketHistory&) = delete;

  ~RtpPacketHistory();

  // Set/get storage mode. Note that setting the state will clear the history,
  // even if setting the same state as is currently used.
  void SetStorePacketsStatus(StorageMode mode, size_t number_to_store);
  StorageMode GetStorageMode() const;

  // Set RTT, used to avoid premature retransmission and to prevent over-writing
  // a packet in the history before we are reasonably sure it has been received.
  void SetRtt(base::TimeDelta rtt);

  void PutRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                    base::Timestamp send_time);

  // Gets stored RTP packet corresponding to the input |sequence number|.
  // Returns nullptr if packet is not found or was (re)sent too recently.
  // If a packet copy is returned, it will be marked as pending transmission but
  // does not update send time, that must be done by MarkPacketAsSent().
  std::unique_ptr<RtpPacketToSend> GetPacketAndMarkAsPending(
      uint16_t sequence_number);

  // In addition to getting packet and marking as sent, this method takes an
  // encapsulator function that takes a reference to the packet and outputs a
  // copy that may be wrapped in a container, eg RTX.
  // If the the encapsulator returns nullptr, the retransmit is aborted and the
  // packet will not be marked as pending.
  std::unique_ptr<RtpPacketToSend> GetPacketAndMarkAsPending(
      uint16_t sequence_number,
      base::FunctionView<std::unique_ptr<RtpPacketToSend>(
          const RtpPacketToSend&)> encapsulate);

  // Updates the send time for the given packet and increments the transmission
  // counter. Marks the packet as no longer being in the pacer queue.
  void MarkPacketAsSent(uint16_t sequence_number);

  // Returns true if history contains packet with `sequence_number` and it can
  // be retransmitted.
  bool GetPacketState(uint16_t sequence_number) const;

  // Get the packet (if any) from the history, that is deemed most likely to
  // the remote side. This is calculated from heuristics such as packet age
  // and times retransmitted. Updated the send time of the packet, so is not
  // a const method.
  std::unique_ptr<RtpPacketToSend> GetPayloadPaddingPacket();

  // Same as GetPayloadPaddingPacket(void), but adds an encapsulation
  // that can be used for instance to encapsulate the packet in an RTX
  // container, or to abort getting the packet if the function returns
  // nullptr.
  std::unique_ptr<RtpPacketToSend> GetPayloadPaddingPacket(
      base::FunctionView<std::unique_ptr<RtpPacketToSend>(
          const RtpPacketToSend&)> encapsulate);

  // Cull packets that have been acknowledged as received by the remote end.
  void CullAcknowledgedPackets(std::span<const uint16_t> sequence_numbers);

  // Remove all pending packets from the history, but keep storage mode and
  // capacity.
  void Clear();

 private:
  class StoredPacket {
   public:
    StoredPacket() = default;
    StoredPacket(std::unique_ptr<RtpPacketToSend> packet,
                 base::Timestamp send_time,
                 uint64_t insert_order);
    StoredPacket(StoredPacket&&);
    StoredPacket& operator=(StoredPacket&&);
    ~StoredPacket();

    uint64_t insert_order() const { return insert_order_; }
    size_t times_retransmitted() const { return times_retransmitted_; }
    void IncrementTimesRetransmitted();

    // The time of last transmission, including retransmissions.
    base::Timestamp send_time() const { return send_time_; }
    void set_send_time(base::Timestamp value) { send_time_ = value; }

    // The actual packet.
    std::unique_ptr<RtpPacketToSend> packet_;

    // True if the packet is currently in the pacer queue pending transmission.
    bool pending_transmission_;

   private:
    base::Timestamp send_time_ = base::Timestamp::Zero();

    // Unique number per StoredPacket, incremented by one for each added
    // packet. Used to sort on insert order.
    uint64_t insert_order_;

    // Number of times RE-transmitted, ie excluding the first transmission.
    size_t times_retransmitted_;
  };

  // Helper method to check if packet has too recently been sent.
  bool VerifyRtt(const StoredPacket& packet) const;
  void Reset();
  void CullOldPackets();
  // Removes the packet from the history, and context/mapping that has been
  // stored. Returns the RTP packet instance contained within the StoredPacket.
  std::unique_ptr<RtpPacketToSend> RemovePacket(int32_t packet_index);
  int32_t GetPacketIndex(uint16_t sequence_number) const;
  StoredPacket* GetStoredPacket(uint16_t sequence_number);

  base::Clock* const clock_;
  const PaddingMode padding_mode_;
  mutable std::mutex lock_;
  size_t number_to_store_;
  StorageMode mode_;
  base::TimeDelta rtt_;

  // Queue of stored packets, ordered by sequence number, with older packets in
  // the front and new packets being added to the back. Note that there may be
  // wrap-arounds so the back may have a lower sequence number.
  // Packets may also be removed out-of-order, in which case there will be
  // instances of StoredPacket with `packet_` set to nullptr. The first and last
  // entry in the queue will however always be populated.
  std::deque<StoredPacket> packet_history_;

  // Total number of packets with inserted.
  uint64_t packets_inserted_;

  std::optional<RtpPacketToSend> large_payload_packet_;
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_
