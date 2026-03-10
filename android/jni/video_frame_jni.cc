/*
 * video_frame_jni.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <jni.h>

#include "base/logging.h"
#include "jni_headers/media/android/generated_media_jni/VideoFrame_jni.h"
#include "third_party/jni_zero/jni_zero.h"

namespace ave {
namespace media {
namespace jni {

jni_zero::ScopedJavaLocalRef<jobject> CreateJavaVideoFrame(JNIEnv* env,
                                                           int width,
                                                           int height,
                                                           int stride,
                                                           int64_t timestamp_us,
                                                           int rotation,
                                                           const uint8_t* data,
                                                           size_t data_size) {
  // Create a direct ByteBuffer wrapping the frame data
  jobject raw_buffer =
      env->NewDirectByteBuffer(const_cast<uint8_t*>(data), data_size);
  if (!raw_buffer) {
    AVE_LOG(LS_ERROR) << "Failed to create ByteBuffer for VideoFrame";
    return jni_zero::ScopedJavaLocalRef<jobject>();
  }
  jni_zero::ScopedJavaLocalRef<jobject> byte_buffer =
      jni_zero::ScopedJavaLocalRef<jobject>::Adopt(env, raw_buffer);

  auto j_frame = Java_VideoFrame_Constructor(
      env, width, height, stride, timestamp_us, rotation, byte_buffer);
  return j_frame;
}

}  // namespace jni
}  // namespace media
}  // namespace ave
