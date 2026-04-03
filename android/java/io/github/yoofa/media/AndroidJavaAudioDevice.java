/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

/**
 * Default {@link AudioDevice} implementation that creates
 * {@link DefaultAudioSink} instances for standard PCM playback.
 */
public class AndroidJavaAudioDevice implements AudioDevice {

    @Override
    public AudioSink createAudioSink() {
        return new DefaultAudioSink();
    }
}
