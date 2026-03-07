/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

/**
 * Interface for rendering video frames.
 * Implement this to display video output (e.g. SurfaceView, TextureView).
 */
public interface VideoRenderer {
    /**
     * Called when a new video frame is available for rendering.
     *
     * @param frame The video frame to render. The implementation should call
     *              frame.close() when done if it holds a reference.
     */
    void onFrame(VideoFrame frame);
}
