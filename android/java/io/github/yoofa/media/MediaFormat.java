/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

import org.jni_zero.CalledByNative;

/**
 * Describes the format of media content (audio or video track).
 * Mirrors the native MediaMeta.
 */
public class MediaFormat {

    /** Media type constants. */
    public static final int MEDIA_TYPE_AUDIO = 0;
    public static final int MEDIA_TYPE_VIDEO = 1;

    private final int mediaType;
    private String mimeType;

    // Video properties
    private int width;
    private int height;
    private int frameRate;
    private int rotation;

    // Audio properties
    private int sampleRate;
    private int channelCount;
    private int bitsPerSample;

    // Common properties
    private int bitrate;
    private long durationUs;

    @CalledByNative
    public MediaFormat(int mediaType) {
        this.mediaType = mediaType;
    }

    /**
     * @return The media type (MEDIA_TYPE_AUDIO or MEDIA_TYPE_VIDEO).
     */
    public int getMediaType() {
        return mediaType;
    }

    public String getMimeType() {
        return mimeType;
    }

    @CalledByNative
    public void setMimeType(String mimeType) {
        this.mimeType = mimeType;
    }

    public int getWidth() {
        return width;
    }

    @CalledByNative
    public void setWidth(int width) {
        this.width = width;
    }

    public int getHeight() {
        return height;
    }

    @CalledByNative
    public void setHeight(int height) {
        this.height = height;
    }

    public int getFrameRate() {
        return frameRate;
    }

    @CalledByNative
    public void setFrameRate(int frameRate) {
        this.frameRate = frameRate;
    }

    public int getRotation() {
        return rotation;
    }

    @CalledByNative
    public void setRotation(int rotation) {
        this.rotation = rotation;
    }

    public int getSampleRate() {
        return sampleRate;
    }

    @CalledByNative
    public void setSampleRate(int sampleRate) {
        this.sampleRate = sampleRate;
    }

    public int getChannelCount() {
        return channelCount;
    }

    @CalledByNative
    public void setChannelCount(int channelCount) {
        this.channelCount = channelCount;
    }

    public int getBitsPerSample() {
        return bitsPerSample;
    }

    @CalledByNative
    public void setBitsPerSample(int bitsPerSample) {
        this.bitsPerSample = bitsPerSample;
    }

    public int getBitrate() {
        return bitrate;
    }

    @CalledByNative
    public void setBitrate(int bitrate) {
        this.bitrate = bitrate;
    }

    public long getDurationUs() {
        return durationUs;
    }

    @CalledByNative
    public void setDurationUs(long durationUs) {
        this.durationUs = durationUs;
    }

    @Override
    public String toString() {
        if (mediaType == MEDIA_TYPE_VIDEO) {
            return "MediaFormat{video, " + mimeType + ", " + width + "x" + height
                    + ", " + frameRate + "fps, " + bitrate + "bps}";
        } else {
            return "MediaFormat{audio, " + mimeType + ", " + sampleRate + "Hz, "
                    + channelCount + "ch, " + bitsPerSample + "bit}";
        }
    }
}
