#ifndef MEDIA_MODULES_RTP_SESSION_TRANSPORT_PACKET_TRANSPORT_SINK_H_
#define MEDIA_MODULES_RTP_SESSION_TRANSPORT_PACKET_TRANSPORT_SINK_H_

#include <cstdint>
#include <span>

#include "base/units/timestamp.h"
#include "media/modules/rtp_session/transport/packet_transport.h"

namespace ave::media::rtp_session {

class PacketTransportSink {
 public:
  virtual ~PacketTransportSink() = default;

  virtual void OnPacketReceived(std::span<const uint8_t> packet,
                                const PacketEndpoint& source,
                                base::Timestamp arrival_time) = 0;
};

}  // namespace ave::media::rtp_session

#endif  // MEDIA_MODULES_RTP_SESSION_TRANSPORT_PACKET_TRANSPORT_SINK_H_
