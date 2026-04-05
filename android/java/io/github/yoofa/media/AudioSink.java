/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

import java.nio.ByteBuffer;
import org.jni_zero.CalledByNative;

/**
 * Audio sink interface called from native code via JNI.
 *
 * <p>This corresponds to the native C++ {@code AudioTrack} interface. The native {@code
 * JavaAudioTrack} calls these methods through JNI to control audio output on Android.
 *
 * <p>Implementations should wrap a platform audio sink (e.g., {@link android.media.AudioTrack}) and
 * handle PCM streaming. Future implementations may support offload or tunnel audio modes.
 *
 * @see DefaultAudioSink
 */
public interface AudioSink {

  /** Audio encoding constants matching native audio_format_t PCM sub-types. */
  int ENCODING_PCM_16BIT = 1;

  int ENCODING_PCM_FLOAT = 2;
  int ENCODING_PCM_8BIT = 3;
  int ENCODING_PCM_32BIT = 4;

  /** Compressed audio encoding constants for passthrough/offload. */
  int ENCODING_AC3 = 10;

  int ENCODING_E_AC3 = 11;
  int ENCODING_DTS = 12;
  int ENCODING_DTS_HD = 13;
  int ENCODING_AAC_LC = 14;
  int ENCODING_DOLBY_TRUEHD = 15;
  int ENCODING_E_AC3_JOC = 16;
  int ENCODING_AC4 = 17;

  /**
   * Open an audio track with the given configuration.
   *
   * @param sampleRate Sample rate in Hz.
   * @param channelCount Number of audio channels.
   * @param encoding One of the ENCODING_* constants.
   * @return true if the track was opened successfully.
   */
  @CalledByNative
  boolean open(int sampleRate, int channelCount, int encoding);

  /**
   * Write audio data to the track.
   *
   * @param data Audio data buffer.
   * @param size Number of bytes to write.
   * @return Number of bytes actually written, or negative on error.
   */
  @CalledByNative
  int write(ByteBuffer data, int size);

  /** Start playback. */
  @CalledByNative
  void start();

  /** Stop playback and reset position. */
  @CalledByNative
  void stop();

  /** Flush buffered data. */
  @CalledByNative
  void flush();

  /** Pause playback. */
  @CalledByNative
  void pause();

  /** Close the track and release resources. */
  @CalledByNative
  void close();

  /**
   * @return true if the track is open and ready to accept data.
   */
  @CalledByNative
  boolean isReady();

  /**
   * @return Buffer size in bytes.
   */
  @CalledByNative
  int getBufferSize();

  /**
   * @return Sample rate in Hz.
   */
  @CalledByNative
  int getSampleRate();

  /**
   * @return Number of audio channels.
   */
  @CalledByNative
  int getChannelCount();

  /**
   * @return Estimated latency in milliseconds.
   */
  @CalledByNative
  int getLatency();

  /**
   * @return Estimated duration of audio data buffered but not yet played, in microseconds.
   */
  @CalledByNative
  long getBufferDurationUs();

  /**
   * @return The number of frames written to the track since open.
   */
  @CalledByNative
  long getFramesWritten();

  /**
   * @return Playback head position in frames since open.
   */
  @CalledByNative
  int getPosition();
}
