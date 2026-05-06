/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/api/rtp_header_extension_map.h"
#include <span>

#include <cstdint>
#include <utility>

#include "base/checks.h"
#include "base/logging.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

namespace ave {
namespace media {
namespace rtp_rtcp {
namespace {

struct ExtensionInfo {
  RTPExtensionType type;
  std::string_view uri;
};

// List of known extensions
constexpr ExtensionInfo kExtensions[] = {
    {.type = kRtpExtensionTransmissionTimeOffset,
     .uri = RtpExtension::kTimestampOffsetUri},
    {.type = kRtpExtensionAudioLevel, .uri = RtpExtension::kAudioLevelUri},
    {.type = kRtpExtensionCsrcAudioLevel,
     .uri = RtpExtension::kCsrcAudioLevelsUri},
    {.type = kRtpExtensionAbsoluteSendTime,
     .uri = RtpExtension::kAbsSendTimeUri},
    {.type = kRtpExtensionAbsoluteCaptureTime,
     .uri = RtpExtension::kAbsoluteCaptureTimeUri},
    {.type = kRtpExtensionVideoRotation,
     .uri = RtpExtension::kVideoRotationUri},
    {.type = kRtpExtensionTransportSequenceNumber,
     .uri = RtpExtension::kTransportSequenceNumberUri},
    {.type = kRtpExtensionTransportSequenceNumber02,
     .uri = RtpExtension::kTransportSequenceNumberV2Uri},
    {.type = kRtpExtensionPlayoutDelay, .uri = RtpExtension::kPlayoutDelayUri},
    {.type = kRtpExtensionVideoContentType,
     .uri = RtpExtension::kVideoContentTypeUri},
    {.type = kRtpExtensionVideoLayersAllocation,
     .uri = RtpExtension::kVideoLayersAllocationUri},
    {.type = kRtpExtensionVideoTiming, .uri = RtpExtension::kVideoTimingUri},
    {.type = kRtpExtensionRtpStreamId, .uri = RtpExtension::kRidUri},
    {.type = kRtpExtensionRepairedRtpStreamId,
     .uri = RtpExtension::kRepairedRidUri},
    {.type = kRtpExtensionMid, .uri = RtpExtension::kMidUri},
    {.type = kRtpExtensionGenericFrameDescriptor,
     .uri = RtpExtension::kGenericFrameDescriptorUri00},
    {.type = kRtpExtensionDependencyDescriptor,
     .uri = RtpExtension::kDependencyDescriptorUri},
    {.type = kRtpExtensionColorSpace, .uri = RtpExtension::kColorSpaceUri},
    {.type = kRtpExtensionVideoFrameTrackingId,
     .uri = RtpExtension::kVideoFrameTrackingIdUri},
    {.type = kRtpExtensionCorruptionDetection,
     .uri = RtpExtension::kCorruptionDetectionUri},
};

}  // namespace

RtpHeaderExtensionMap::RtpHeaderExtensionMap() : RtpHeaderExtensionMap(false) {}

RtpHeaderExtensionMap::RtpHeaderExtensionMap(bool extmap_allow_mixed)
    : extmap_allow_mixed_(extmap_allow_mixed) {
  for (auto& id : ids_) {
    id = kInvalidId;
  }
}

RtpHeaderExtensionMap::RtpHeaderExtensionMap(
    std::span<const RtpExtension> extensions)
    : RtpHeaderExtensionMap(false) {
  for (const RtpExtension& extension : extensions) {
    RegisterByUri(extension.id, extension.uri);
  }
}

void RtpHeaderExtensionMap::Reset(std::span<const RtpExtension> extensions) {
  for (auto& id : ids_) {
    id = kInvalidId;
  }
  for (const RtpExtension& extension : extensions) {
    RegisterByUri(extension.id, extension.uri);
  }
}

bool RtpHeaderExtensionMap::RegisterByType(int32_t id, RTPExtensionType type) {
  for (const ExtensionInfo& extension : kExtensions) {
    if (type == extension.type) {
      return Register(id, extension.type, extension.uri);
    }
  }
  AVE_DCHECK(false);
  return false;
}

bool RtpHeaderExtensionMap::RegisterByUri(int32_t id, std::string_view uri) {
  for (const ExtensionInfo& extension : kExtensions) {
    if (uri == extension.uri) {
      return Register(id, extension.type, extension.uri);
    }
  }
  AVE_LOG(LS_WARNING) << "Unknown extension uri:'" << uri << "', id: " << id
                      << '.';
  return false;
}

RTPExtensionType RtpHeaderExtensionMap::GetType(int32_t id) const {
  AVE_DCHECK_GE(id, RtpExtension::kMinId);
  AVE_DCHECK_LE(id, RtpExtension::kMaxId);
  for (int32_t type = kRtpExtensionNone + 1;
       type < kRtpExtensionNumberOfExtensions; ++type) {
    if (std::cmp_equal(ids_[type], id)) {
      return static_cast<RTPExtensionType>(type);
    }
  }
  return kInvalidType;
}

void RtpHeaderExtensionMap::Deregister(std::string_view uri) {
  for (const ExtensionInfo& extension : kExtensions) {
    if (extension.uri == uri) {
      ids_[extension.type] = kInvalidId;
      break;
    }
  }
}

bool RtpHeaderExtensionMap::Register(int32_t id,
                                     RTPExtensionType type,
                                     std::string_view uri) {
  AVE_DCHECK_GT(type, kRtpExtensionNone);
  AVE_DCHECK_LT(type, kRtpExtensionNumberOfExtensions);

  if (id < RtpExtension::kMinId || id > RtpExtension::kMaxId) {
    AVE_LOG(LS_WARNING) << "Failed to register extension uri:'" << uri
                        << "' with invalid id:" << id << ".";
    return false;
  }

  RTPExtensionType registered_type = GetType(id);
  if (registered_type == type) {  // Same type/id pair already registered.
    AVE_LOG(LS_VERBOSE) << "Reregistering extension uri:'" << uri
                        << "', id:" << id;
    return true;
  }

  if (registered_type !=
      kInvalidType) {  // `id` used by another extension type.
    AVE_LOG(LS_WARNING) << "Failed to register extension uri:'" << uri
                        << "', id:" << id
                        << ". Id already in use by extension type "
                        << static_cast<int32_t>(registered_type);
    return false;
  }
  if (IsRegistered(type)) {
    AVE_LOG(LS_WARNING) << "Illegal reregistration for uri: " << uri
                        << " is previously registered with id " << GetId(type)
                        << " and cannot be reregistered with id " << id;
    return false;
  }

  // There is a run-time check above id fits into uint8_t.
  ids_[type] = static_cast<uint8_t>(id);
  return true;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
