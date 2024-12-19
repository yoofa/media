/*
 * video_render.h
 * Copyright (C) 2022 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef VIDEO_RENDER_H
#define VIDEO_RENDER_H

#include <memory>

#include "media/foundation/media_sink_base.h"
#include "media/foundation/message_object.h"

namespace ave {
namespace media {

class VideoRender : public MediaFrameSink, public MessageObject {
 public:
  VideoRender() = default;
  ~VideoRender() override = default;

  // MediaSinkInterface implementation
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override = 0;

  virtual int64_t render_latency() { return 0; }
};

}  // namespace media
}  // namespace ave

#endif /* !VIDEO_RENDER_H */
