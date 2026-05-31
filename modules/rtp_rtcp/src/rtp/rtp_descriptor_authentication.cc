/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Ported to AVE from WebRTC.
 */

#include "media/modules/rtp_rtcp/src/rtp/rtp_descriptor_authentication.h"

#include <cstdint>
#include <vector>

#include <span>
#include "media/modules/rtp_rtcp/src/rtp/rtp_generic_frame_descriptor.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_generic_frame_descriptor_extension.h"
#include "media/modules/rtp_rtcp/src/rtp/rtp_video_header.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

std::vector<uint8_t> RtpDescriptorAuthentication(
    const RTPVideoHeader& rtp_video_header) {
  if (!rtp_video_header.generic) {
    return {};
  }
  const RTPVideoHeader::GenericDescriptorInfo& descriptor =
      *rtp_video_header.generic;
  // Default way of creating additional data for an encrypted frame.
  if (descriptor.spatial_index < 0 || descriptor.temporal_index < 0 ||
      descriptor.spatial_index >=
          RtpGenericFrameDescriptor::kMaxSpatialLayers ||
      descriptor.temporal_index >=
          RtpGenericFrameDescriptor::kMaxTemporalLayers ||
      descriptor.dependencies.size() >
          RtpGenericFrameDescriptor::kMaxNumFrameDependencies) {
    return {};
  }
  RtpGenericFrameDescriptor frame_descriptor;
  frame_descriptor.SetFirstPacketInSubFrame(true);
  frame_descriptor.SetLastPacketInSubFrame(false);
  frame_descriptor.SetTemporalLayer(descriptor.temporal_index);
  frame_descriptor.SetSpatialLayersBitmask(1 << descriptor.spatial_index);
  frame_descriptor.SetFrameId(descriptor.frame_id & 0xFFFF);
  for (int64_t dependency : descriptor.dependencies) {
    frame_descriptor.AddFrameDependencyDiff(descriptor.frame_id - dependency);
  }
  if (descriptor.dependencies.empty()) {
    frame_descriptor.SetResolution(rtp_video_header.width,
                                   rtp_video_header.height);
  }
  std::vector<uint8_t> result(
      RtpGenericFrameDescriptorExtension00::ValueSize(frame_descriptor));
  RtpGenericFrameDescriptorExtension00::Write(
      std::span<uint8_t>(result.data(), result.size()), frame_descriptor);
  return result;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
