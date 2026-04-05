/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Build;
import android.util.Log;

/**
 * Default {@link AudioDevice} implementation that creates {@link DefaultAudioSink} instances for
 * standard PCM playback.
 */
public class AndroidJavaAudioDevice implements AudioDevice {

  private static final String TAG = "AndroidJavaAudioDevice";

  @Override
  public AudioSink createAudioSink() {
    return new DefaultAudioSink();
  }

  @Override
  public boolean isEncodingSupported(int encoding) {
    int androidEncoding = toAndroidEncoding(encoding);
    if (androidEncoding == AudioFormat.ENCODING_INVALID) {
      return false;
    }

    // PCM formats are always supported.
    if (encoding <= AudioSink.ENCODING_PCM_32BIT) {
      return true;
    }

    // API 29+: use AudioTrack.isDirectPlaybackSupported()
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      try {
        AudioFormat format =
            new AudioFormat.Builder()
                .setEncoding(androidEncoding)
                .setSampleRate(48000)
                .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                .build();
        AudioAttributes attrs =
            new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
                .build();
        boolean supported = AudioTrack.isDirectPlaybackSupported(format, attrs);
        Log.i(
            TAG,
            "isEncodingSupported: encoding="
                + encoding
                + " androidEncoding="
                + androidEncoding
                + " supported="
                + supported);
        return supported;
      } catch (Exception e) {
        Log.w(TAG, "isDirectPlaybackSupported failed", e);
        return false;
      }
    }

    // Pre-API 29: conservatively return false for compressed.
    Log.i(TAG, "isEncodingSupported: API < 29, returning false for encoding=" + encoding);
    return false;
  }

  private static int toAndroidEncoding(int avpEncoding) {
    switch (avpEncoding) {
      case AudioSink.ENCODING_PCM_16BIT:
        return AudioFormat.ENCODING_PCM_16BIT;
      case AudioSink.ENCODING_PCM_FLOAT:
        return AudioFormat.ENCODING_PCM_FLOAT;
      case AudioSink.ENCODING_PCM_8BIT:
        return AudioFormat.ENCODING_PCM_8BIT;
      case AudioSink.ENCODING_PCM_32BIT:
        return AudioFormat.ENCODING_PCM_32BIT;
      case AudioSink.ENCODING_AC3:
        return AudioFormat.ENCODING_AC3;
      case AudioSink.ENCODING_E_AC3:
        return AudioFormat.ENCODING_E_AC3;
      case AudioSink.ENCODING_DTS:
        return AudioFormat.ENCODING_DTS;
      case AudioSink.ENCODING_DTS_HD:
        return AudioFormat.ENCODING_DTS_HD;
      case AudioSink.ENCODING_AAC_LC:
        return AudioFormat.ENCODING_AAC_LC;
      case AudioSink.ENCODING_DOLBY_TRUEHD:
        return AudioFormat.ENCODING_DOLBY_TRUEHD;
      case AudioSink.ENCODING_E_AC3_JOC:
        return AudioFormat.ENCODING_E_AC3_JOC;
      case AudioSink.ENCODING_AC4:
        return AudioFormat.ENCODING_AC4;
      default:
        return AudioFormat.ENCODING_INVALID;
    }
  }
}
