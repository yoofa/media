/*
 * android_native_window_render.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_CODEC_ANDROID_ANDROID_NATIVE_WINDOW_RENDER_H_
#define MEDIA_CODEC_ANDROID_ANDROID_NATIVE_WINDOW_RENDER_H_

#include <android/native_window.h>

#include "media/video/video_render.h"

namespace ave {
namespace media {

/**
 * @brief Android-specific VideoRender that owns an ANativeWindow.
 *
 * Serves two purposes:
 * 1. **Surface mode** (used by AndroidNdkMediaCodec): the codec decodes
 *    directly to the surface.  OnFrame() receives metadata-only frames (no
 *    pixel data) and is a no-op.
 * 2. **Buffer mode fallback**: OnFrame() receives frames with raw pixel data
 *    (YUV420P or NV12) and performs software conversion to RGBA before
 *    drawing to the ANativeWindow.
 *
 * AndroidNdkMediaCodec detects this class via dynamic_cast to retrieve
 * the ANativeWindow* for hardware surface rendering configuration.
 */
class AndroidNativeWindowRender : public VideoRender {
 public:
  /**
   * @brief Constructs the render, acquiring a reference to the native window.
   * @param window The ANativeWindow to render into. Must not be null.
   *               An extra reference is acquired; the caller may release
   *               their own reference after construction.
   */
  explicit AndroidNativeWindowRender(ANativeWindow* window);

  /**
   * @brief Destructor. Releases the ANativeWindow reference.
   */
  ~AndroidNativeWindowRender() override;

  /**
   * @brief Returns the underlying ANativeWindow.
   *
   * Overrides VideoRender::android_native_window() for no-RTTI detection
   * by AndroidNdkMediaCodec.
   *
   * @return Pointer to the ANativeWindow, or nullptr if not set.
   */
  ANativeWindow* android_native_window() const override {
    return native_window_;
  }

  /**
   * @brief Renders a video frame to the ANativeWindow.
   *
   * If the frame has no pixel data (surface mode), this is a no-op.
   * Otherwise, performs software YUV420P or NV12 → RGBA conversion.
   *
   * @param frame The video frame to render.
   */
  void OnFrame(const std::shared_ptr<MediaFrame>& frame) override;

 private:
  ANativeWindow* native_window_;
};

}  // namespace media
}  // namespace ave

#endif  // MEDIA_CODEC_ANDROID_ANDROID_NATIVE_WINDOW_RENDER_H_
