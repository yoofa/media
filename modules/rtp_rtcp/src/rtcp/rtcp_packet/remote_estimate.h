/*
 * remote_estimate.h - Remote estimate RTCP packet
 *
 * Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-oriented framework.
 */

#ifndef MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMOTE_ESTIMATE_H_
#define MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMOTE_ESTIMATE_H_

#include <memory>
#include <span>
#include <vector>

#include "base/buffer.h"
#include "media/modules/rtp_rtcp/src/rtcp/rtcp_packet/app.h"
#include "media/modules/rtp_rtcp/src/transport/network_types.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace rtcp {

class CommonHeader;

class RemoteEstimateSerializer {
 public:
  virtual bool Parse(std::span<const uint8_t> src,
                     NetworkStateEstimate* target) const = 0;
  virtual base::Buffer Serialize(const NetworkStateEstimate& src) const = 0;
  virtual ~RemoteEstimateSerializer() = default;
};

// Using a static global implementation to avoid incurring initialization
// overhead of the serializer every time RemoteEstimate is created.
const RemoteEstimateSerializer* GetRemoteEstimateSerializer();

// The RemoteEstimate packet provides network estimation results from the
// receive side. This functionality is experimental and subject to change
// without notice.
class RemoteEstimate : public App {
 public:
  RemoteEstimate();
  explicit RemoteEstimate(App&& app);
  // Note, sub type must be unique among all app messages with "goog" name.
  static constexpr uint8_t kSubType = 13;
  static constexpr uint32_t kName = NameToInt("goog");

  bool ParseData();
  void SetEstimate(NetworkStateEstimate estimate);
  NetworkStateEstimate estimate() const { return estimate_; }

 private:
  NetworkStateEstimate estimate_;
  const RemoteEstimateSerializer* const serializer_;
};

}  // namespace rtcp
}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // MEDIA_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMOTE_ESTIMATE_H_
