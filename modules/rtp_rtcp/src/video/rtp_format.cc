/*
 * rtp_format.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/modules/rtp_rtcp/src/video/rtp_format.h"

#include "base/checks.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

std::vector<int> RtpPacketizer::SplitAboutEqually(
    int payload_len,
    const PayloadSizeLimits& limits) {
  AVE_DCHECK_GT(payload_len, 0);
  // First or last packet larger than normal are unsupported.
  AVE_DCHECK_GE(limits.first_packet_reduction_len, 0);
  AVE_DCHECK_GE(limits.last_packet_reduction_len, 0);

  std::vector<int> result;
  if (limits.max_payload_len >=
      limits.single_packet_reduction_len + payload_len) {
    result.push_back(payload_len);
    return result;
  }
  if (limits.max_payload_len - limits.first_packet_reduction_len < 1 ||
      limits.max_payload_len - limits.last_packet_reduction_len < 1) {
    // Capacity is not enough to put a single byte into one of the packets.
    return result;
  }
  // First and last packet of the frame can be smaller. Pretend that it's
  // the same size, but we must write more payload to it.
  // Assume frame fits in single packet if packet has extra space for sum
  // of first and last packets reductions.
  int total_bytes = payload_len + limits.first_packet_reduction_len +
                    limits.last_packet_reduction_len;
  // Integer divisions with rounding up.
  int num_packets_left =
      (total_bytes + limits.max_payload_len - 1) / limits.max_payload_len;
  if (num_packets_left == 1) {
    // Single packet is a special case handled above.
    num_packets_left = 2;
  }

  if (payload_len < num_packets_left) {
    // Edge case where limits force to have more packets than there are payload
    // bytes. This may happen when there is single byte of payload that can't be
    // put into single packet if
    // first_packet_reduction + last_packet_reduction >= max_payload_len.
    return result;
  }

  int bytes_per_packet = total_bytes / num_packets_left;
  int num_larger_packets = total_bytes % num_packets_left;
  int remaining_data = payload_len;

  result.reserve(num_packets_left);
  bool first_packet = true;
  while (remaining_data > 0) {
    // Last num_larger_packets are 1 byte wider than the rest. Increase
    // per-packet payload size when needed.
    if (num_packets_left == num_larger_packets)
      ++bytes_per_packet;
    int current_packet_bytes = bytes_per_packet;
    if (first_packet) {
      if (current_packet_bytes > limits.first_packet_reduction_len + 1)
        current_packet_bytes -= limits.first_packet_reduction_len;
      else
        current_packet_bytes = 1;
    }
    if (current_packet_bytes > remaining_data) {
      current_packet_bytes = remaining_data;
    }
    // This is not the last packet in the whole payload, but there's no data
    // left for the last packet. Leave at least one byte for the last packet.
    if (num_packets_left == 2 && current_packet_bytes == remaining_data) {
      --current_packet_bytes;
    }
    result.push_back(current_packet_bytes);

    remaining_data -= current_packet_bytes;
    --num_packets_left;
    first_packet = false;
  }

  return result;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
