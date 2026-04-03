/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

import org.jni_zero.CalledByNative;

/**
 * Audio device factory interface called from native code via JNI.
 *
 * <p>This corresponds to the native C++ {@code AudioDevice} interface. The
 * native {@code JavaAudioDevice} calls {@link #createAudioSink()} through
 * JNI to obtain an {@link AudioSink} for audio output.
 *
 * <p>Implementations decide which {@link AudioSink} to create (e.g.,
 * {@link DefaultAudioSink} for standard PCM playback, or a custom sink
 * for offload/tunnel modes).
 *
 * @see AndroidJavaAudioDevice
 */
public interface AudioDevice {

    /**
     * Create a new {@link AudioSink} for audio playback.
     *
     * <p>Called from native code each time an audio track is needed.
     * The returned sink is not yet opened; native code will call
     * {@link AudioSink#open} to configure it.
     *
     * @return A new AudioSink instance, or null on failure.
     */
    @CalledByNative
    AudioSink createAudioSink();
}
