/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

import java.nio.ByteBuffer;

import org.jni_zero.CalledByNative;

/**
 * Represents a decoded video frame from the native player.
 * Wraps a native MediaFrame for rendering.
 */
public class VideoFrame implements AutoCloseable {
    private final int width;
    private final int height;
    private final int stride;
    private final long timestampUs;
    private final int rotation;
    private ByteBuffer buffer;

    @CalledByNative
    public VideoFrame(int width, int height, int stride, long timestampUs,
                      int rotation, ByteBuffer buffer) {
        this.width = width;
        this.height = height;
        this.stride = stride;
        this.timestampUs = timestampUs;
        this.rotation = rotation;
        this.buffer = buffer;
    }

    /**
     * @return Frame width in pixels.
     */
    public int getWidth() {
        return width;
    }

    /**
     * @return Frame height in pixels.
     */
    public int getHeight() {
        return height;
    }

    /**
     * @return Y-plane stride in bytes.
     */
    public int getStride() {
        return stride;
    }

    /**
     * @return Presentation timestamp in microseconds.
     */
    public long getTimestampUs() {
        return timestampUs;
    }

    /**
     * @return Frame rotation in degrees (0, 90, 180, 270).
     */
    public int getRotation() {
        return rotation;
    }

    /**
     * @return The I420 pixel data buffer, or null if released.
     */
    public ByteBuffer getBuffer() {
        return buffer;
    }

    @Override
    public void close() {
        buffer = null;
    }
}
