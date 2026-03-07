/*
 * video_frame_jni.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_ANDROID_JNI_VIDEO_FRAME_JNI_H_
#define MEDIA_ANDROID_JNI_VIDEO_FRAME_JNI_H_

#include <jni.h>
#include <cstdint>

#include "third_party/jni_zero/java_refs.h"

namespace ave {
namespace media {
namespace jni {

/**
 * @brief Create a Java VideoFrame object from native frame data.
 * @param env JNI environment.
 * @param width Frame width in pixels.
 * @param height Frame height in pixels.
 * @param stride Y-plane stride in bytes.
 * @param timestamp_us Presentation timestamp in microseconds.
 * @param rotation Frame rotation in degrees.
 * @param data Pointer to I420 pixel data.
 * @param data_size Size of the pixel data in bytes.
 * @return A Java VideoFrame object, or null on failure.
 */
jni_zero::ScopedJavaLocalRef<jobject> CreateJavaVideoFrame(
    JNIEnv* env,
    int width,
    int height,
    int stride,
    int64_t timestamp_us,
    int rotation,
    const uint8_t* data,
    size_t data_size);

}  // namespace jni
}  // namespace media
}  // namespace ave

#endif  // MEDIA_ANDROID_JNI_VIDEO_FRAME_JNI_H_
