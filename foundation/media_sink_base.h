/*
 * media_sink_base.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_SINK_BASE_H
#define MEDIA_SINK_BASE_H

#include "media_frame.h"
#include "media_packet.h"
#include "media_source_sink_interface.h"

namespace ave {
namespace media {

class MediaPacketSink
    : public MediaSinkInterface<std::shared_ptr<MediaPacket>> {
 public:
  ~MediaPacketSink() override;

  // MediaSinkInterface
  void OnFrame(const std::shared_ptr<MediaPacket>& packet) override;
};

class MediaFrameSink : public MediaSinkInterface<std::shared_ptr<MediaFrame>> {
 public:
  ~MediaFrameSink() override;

  // MediaSinkInterface
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override;
};

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_SINK_BASE_H */
