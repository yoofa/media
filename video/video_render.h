/*
 * video_render.h
 * Copyright (C) 2022 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef VIDEO_RENDER_H
#define VIDEO_RENDER_H

#if defined(ANDROID)
#include <android/native_window.h>
#endif

#include <memory>

#include "media/foundation/media_sink_base.h"

namespace ave {
namespace media {

class VideoRender : public MediaFrameSink {
 public:
  VideoRender() = default;
  ~VideoRender() override = default;

  // MediaSinkInterface implementation
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override = 0;

  virtual int64_t render_latency() { return 0; }

#if defined(ANDROID)
  /**
   * Returns the ANativeWindow associated with this render sink, if any.
   * Overridden by AndroidNativeWindowRender to enable hardware surface
   * rendering in AndroidNdkMediaCodec.
   * @return ANativeWindow* or nullptr.
   */
  virtual ANativeWindow* android_native_window() const { return nullptr; }
#endif
};

}  // namespace media
}  // namespace ave

#endif /* !VIDEO_RENDER_H */
