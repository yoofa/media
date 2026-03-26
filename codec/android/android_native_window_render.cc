/*
 * android_native_window_render.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "android_native_window_render.h"

#include <android/native_window.h>

#include "base/logging.h"
#include "media/foundation/media_frame.h"
#include "media/foundation/pixel_format.h"

namespace ave {
namespace media {

namespace {

inline uint8_t Clamp(int val) {
  return static_cast<uint8_t>(val < 0 ? 0 : (val > 255 ? 255 : val));
}

// Software YUV420P (I420) planar → RGBA conversion.
void RenderYuv420pToWindow(ANativeWindow* window,
                           const uint8_t* data,
                           int width,
                           int height) {
  ANativeWindow_setBuffersGeometry(window, width, height,
                                   WINDOW_FORMAT_RGBA_8888);
  ANativeWindow_Buffer buffer;
  if (ANativeWindow_lock(window, &buffer, nullptr) != 0) {
    AVE_LOG(LS_ERROR) << "RenderYuv420pToWindow: ANativeWindow_lock failed";
    return;
  }

  const uint8_t* y_plane = data;
  const uint8_t* u_plane = data + width * height;
  const uint8_t* v_plane = u_plane + (width / 2) * (height / 2);
  auto* dst = static_cast<uint8_t*>(buffer.bits);
  int dst_stride = buffer.stride * 4;

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int y = y_plane[row * width + col];
      int u = u_plane[(row / 2) * (width / 2) + (col / 2)];
      int v = v_plane[(row / 2) * (width / 2) + (col / 2)];
      int c = y - 16, d = u - 128, e = v - 128;
      uint8_t* pixel = dst + row * dst_stride + col * 4;
      pixel[0] = Clamp((298 * c + 409 * e + 128) >> 8);
      pixel[1] = Clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
      pixel[2] = Clamp((298 * c + 516 * d + 128) >> 8);
      pixel[3] = 255;
    }
  }

  ANativeWindow_unlockAndPost(window);
}

// Software NV12 (YUV420 semi-planar) → RGBA conversion.
// Y plane followed by interleaved UV plane.
void RenderNv12ToWindow(ANativeWindow* window,
                        const uint8_t* data,
                        int width,
                        int height,
                        int stride) {
  ANativeWindow_setBuffersGeometry(window, width, height,
                                   WINDOW_FORMAT_RGBA_8888);
  ANativeWindow_Buffer buffer;
  if (ANativeWindow_lock(window, &buffer, nullptr) != 0) {
    AVE_LOG(LS_ERROR) << "RenderNv12ToWindow: ANativeWindow_lock failed";
    return;
  }

  int src_stride = stride > 0 ? stride : width;
  const uint8_t* y_plane = data;
  const uint8_t* uv_plane = data + src_stride * height;
  auto* dst = static_cast<uint8_t*>(buffer.bits);
  int dst_stride = buffer.stride * 4;

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      int y = y_plane[row * src_stride + col];
      int uv_offset = (row / 2) * src_stride + (col & ~1);
      int u = uv_plane[uv_offset];
      int v = uv_plane[uv_offset + 1];
      int c = y - 16, d = u - 128, e = v - 128;
      uint8_t* pixel = dst + row * dst_stride + col * 4;
      pixel[0] = Clamp((298 * c + 409 * e + 128) >> 8);
      pixel[1] = Clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
      pixel[2] = Clamp((298 * c + 516 * d + 128) >> 8);
      pixel[3] = 255;
    }
  }

  ANativeWindow_unlockAndPost(window);
}

}  // namespace

AndroidNativeWindowRender::AndroidNativeWindowRender(ANativeWindow* window)
    : native_window_(window) {
  if (native_window_) {
    ANativeWindow_acquire(native_window_);
    AVE_LOG(LS_INFO) << "AndroidNativeWindowRender: window=" << native_window_;
  }
}

AndroidNativeWindowRender::~AndroidNativeWindowRender() {
  if (native_window_) {
    ANativeWindow_release(native_window_);
    native_window_ = nullptr;
  }
}

void AndroidNativeWindowRender::OnFrame(
    const std::shared_ptr<MediaFrame>& frame) {
  if (!frame || !native_window_)
    return;

  auto* vinfo = frame->video_info();
  if (!vinfo)
    return;

  // Surface mode: codec renders to the window directly; frame carries no pixel
  // data — nothing to do here.
  if (!frame->data() || frame->size() == 0) {
    return;
  }

  int width = vinfo->width;
  int height = vinfo->height;
  if (width <= 0 || height <= 0)
    return;

  if (vinfo->pixel_format == PixelFormat::AVE_PIX_FMT_NV12) {
    RenderNv12ToWindow(native_window_, frame->data(), width, height,
                       vinfo->stride);
  } else {
    // Default: YUV420P planar (FFmpeg software decoder output)
    RenderYuv420pToWindow(native_window_, frame->data(), width, height);
  }
}

}  // namespace media
}  // namespace ave
