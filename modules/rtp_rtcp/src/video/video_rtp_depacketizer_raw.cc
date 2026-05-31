/*
 * video_rtp_depacketizer_raw.cc
 * Copyright (C) 2024 WebRTC project authors. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer_raw.h"

#include <optional>
#include <utility>

#include "base/copy_on_write_buffer.h"
#include "media/modules/rtp_rtcp/src/video/video_rtp_depacketizer.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerRaw::Parse(base::CopyOnWriteBuffer rtp_payload) {
  std::optional<ParsedRtpPayload> parsed(std::in_place);
  parsed->video_payload = std::move(rtp_payload);
  return parsed;
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
