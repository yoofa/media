/*
 *  Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 *  Distributed under terms of the GPLv2 license.
 */

package io.github.yoofa.media;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;
import java.nio.ByteBuffer;

/**
 * Default {@link AudioSink} implementation backed by Android {@link AudioTrack} in streaming mode
 * ({@code MODE_STREAM}).
 *
 * <p>Supports PCM output and compressed direct/offload playback on devices that advertise support
 * for the requested encoding.
 */
public class DefaultAudioSink implements AudioSink {

  private static final String TAG = "DefaultAudioSink";

  private AudioTrack audioTrack;
  private int sampleRate;
  private int channelCount;

  /** PCM-equivalent frames written (for compressed: access-units × samplesPerFrame). */
  private long framesWritten;

  private int frameSize;
  private boolean isCompressed;

  /** Decoded PCM samples per compressed access unit (e.g. 1024 for AAC-LC). */
  private int samplesPerFrame;

  private final AudioTimestamp audioTimestamp = new AudioTimestamp();
  private long lastTimestampFramePosition = -1;
  private long lastTimestampLogRealtimeMs;

  @Override
  public boolean open(int sampleRate, int channelCount, int encoding) {
    Log.i(
        TAG,
        "open: sampleRate=" + sampleRate + " channels=" + channelCount + " encoding=" + encoding);
    close();

    this.sampleRate = sampleRate;
    this.channelCount = channelCount;
    this.framesWritten = 0;
    this.isCompressed = isCompressedEncoding(encoding);
    this.lastTimestampFramePosition = -1;
    this.lastTimestampLogRealtimeMs = 0;

    int androidEncoding = toAndroidEncoding(encoding);
    if (androidEncoding == AudioFormat.ENCODING_INVALID) {
      Log.e(TAG, "Unsupported encoding: " + encoding);
      return false;
    }

    int channelMask = channelCountToMask(channelCount);
    if (isCompressed) {
      // For compressed formats, frame size is per-access-unit (not per-sample).
      this.frameSize = 1;
      this.samplesPerFrame = getSamplesPerFrame(encoding);
    } else {
      int bytesPerSample = bytesPerSampleForEncoding(androidEncoding);
      this.frameSize = channelCount * bytesPerSample;
      this.samplesPerFrame = 0;
    }

    int minBufferSize = AudioTrack.getMinBufferSize(sampleRate, channelMask, androidEncoding);
    if (minBufferSize <= 0) {
      Log.e(TAG, "getMinBufferSize failed: " + minBufferSize);
      return false;
    }

    // Use 4x min buffer for smooth streaming playback
    int bufferSize = minBufferSize * 4;

    try {
      AudioAttributes attrs =
          new AudioAttributes.Builder()
              .setUsage(AudioAttributes.USAGE_MEDIA)
              .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
              .build();

      AudioFormat format =
          new AudioFormat.Builder()
              .setSampleRate(sampleRate)
              .setChannelMask(channelMask)
              .setEncoding(androidEncoding)
              .build();

      AudioTrack.Builder builder =
          new AudioTrack.Builder()
              .setAudioAttributes(attrs)
              .setAudioFormat(format)
              .setBufferSizeInBytes(bufferSize)
              .setTransferMode(AudioTrack.MODE_STREAM)
              .setSessionId(AudioManager.AUDIO_SESSION_ID_GENERATE);
      if (isCompressed && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        builder.setOffloadedPlayback(true);
      }

      audioTrack = builder.build();

      if (audioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
        Log.e(TAG, "AudioTrack failed to initialize");
        audioTrack.release();
        audioTrack = null;
        return false;
      }

      boolean offloaded = false;
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        offloaded = audioTrack.isOffloadedPlayback();
      }
      Log.i(
          TAG,
          "AudioTrack created: bufferSize="
              + bufferSize
              + " minBufferSize="
              + minBufferSize
              + " sampleRate="
              + audioTrack.getSampleRate()
              + " offloaded="
              + offloaded);
      return true;
    } catch (Exception e) {
      Log.e(TAG, "Failed to create AudioTrack", e);
      audioTrack = null;
      return false;
    }
  }

  @Override
  public int write(ByteBuffer data, int size) {
    if (audioTrack == null) {
      return -1;
    }
    if (isCompressed) {
      int startPosition = data.position();
      int written = audioTrack.write(data, size, AudioTrack.WRITE_NON_BLOCKING);
      data.position(startPosition);
      if (written == size) {
        // Count exactly one compressed access unit only after the full AU
        // has been accepted by AudioTrack.
        framesWritten += samplesPerFrame;
      } else if (written > 0) {
        Log.w(
            TAG,
            "write incomplete: requested=" + size + " written=" + written + " compressed=true");
      }
      return written;
    }
    int totalWritten = 0;
    while (totalWritten < size) {
      data.position(totalWritten);
      int written = audioTrack.write(data, size - totalWritten, AudioTrack.WRITE_BLOCKING);
      if (written <= 0) {
        if (totalWritten > 0) {
          Log.w(
              TAG,
              "write incomplete: requested="
                  + size
                  + " written="
                  + totalWritten
                  + " lastResult="
                  + written
                  + " compressed="
                  + isCompressed);
          return totalWritten;
        }
        return written;
      }
      totalWritten += written;
    }
    data.position(0);
    framesWritten += totalWritten / frameSize;
    return totalWritten;
  }

  @Override
  public void start() {
    if (audioTrack != null) {
      Log.i(TAG, "start");
            audioTrack.play();
        }
    }

    @Override
    public void stop() {
        if (audioTrack != null) {
            Log.i(TAG, "stop");
            try {
                audioTrack.stop();
            } catch (IllegalStateException e) {
                Log.w(TAG, "stop failed", e);
            }
            framesWritten = 0;
        }
    }

    @Override
    public void flush() {
        if (audioTrack != null) {
            Log.i(TAG, "flush");
            audioTrack.flush();
            framesWritten = 0;
        }
    }

    @Override
    public void pause() {
        if (audioTrack != null) {
            Log.i(TAG, "pause");
            try {
                audioTrack.pause();
            } catch (IllegalStateException e) {
                Log.w(TAG, "pause failed", e);
            }
        }
    }

    @Override
    public void close() {
        if (audioTrack != null) {
            Log.i(TAG, "close");
            try {
                audioTrack.stop();
            } catch (IllegalStateException ignored) {
            }
            audioTrack.release();
            audioTrack = null;
            framesWritten = 0;
        }
    }

    @Override
    public boolean isReady() {
        return audioTrack != null
                && audioTrack.getState() == AudioTrack.STATE_INITIALIZED;
    }

    @Override
    public int getBufferSize() {
        if (audioTrack == null) {
            return 0;
        }
        return audioTrack.getBufferSizeInFrames() * frameSize;
    }

    @Override
    public int getSampleRate() {
        return sampleRate;
    }

    @Override
    public int getChannelCount() {
        return channelCount;
    }

    @Override
    public int getLatency() {
        if (audioTrack == null || sampleRate == 0) {
            return 0;
        }
        int bufferFrames = audioTrack.getBufferSizeInFrames();
        return (int) ((long) bufferFrames * 1000 / sampleRate);
  }

  @Override
  public long getBufferDurationUs() {
    if (audioTrack == null || sampleRate == 0) {
      return 0;
    }
    if (isCompressed) {
      // For passthrough (compressed) audio, getTimestamp() on many Android
      // HALs reports the write-pointer position (data accepted by the HAL),
      // not the play-pointer (data actually presented to the transducer).
      // This makes frame-count latency unreliable — it always evaluates to 0.
      //
      // Instead, use AudioTrack.getLatency() which queries the HAL for the
      // total output latency including the hardware FIFO depth. This is the
      // actual delay between writing a frame and hearing it.
      return getHardwareLatencyUs();
    }
    long playedFrames = getPlayedFrameCount();
    long buffered = framesWritten - playedFrames;
    if (buffered <= 0) {
      return 0;
    }
    return buffered * 1_000_000L / sampleRate;
  }

    @Override
    public long getFramesWritten() {
        return framesWritten;
  }

  @Override
  public int getPosition() {
    if (audioTrack == null) {
      return 0;
    }
    return (int) getPlayedFrameCount();
  }

  // --- Private helpers ---

  /**
   * Returns the number of PCM-equivalent frames that have been played out. Prefers
   * AudioTrack.getTimestamp() for accuracy (required for compressed formats where
   * getPlaybackHeadPosition() is unreliable), with a fallback to getPlaybackHeadPosition() for PCM
   * when getTimestamp() is unavailable.
   */
  private long getPlayedFrameCount() {
    if (audioTrack == null) {
      return 0;
    }
    try {
      if (audioTrack.getTimestamp(audioTimestamp)) {
        // Extrapolate from the last known position to now.
        long nowNs = System.nanoTime();
        long elapsedNs = nowNs - audioTimestamp.nanoTime;
        if (elapsedNs < 0) {
          elapsedNs = 0;
        }
        long extrapolatedFrames = elapsedNs * sampleRate / 1_000_000_000L;
        if (isCompressed) {
          long rawFramePosition = audioTimestamp.framePosition & 0xFFFFFFFFL;
          long nowMs = SystemClock.elapsedRealtime();
          if (lastTimestampFramePosition >= 0 && rawFramePosition < lastTimestampFramePosition) {
            Log.w(
                TAG,
                "offload timestamp regressed: raw="
                    + rawFramePosition
                    + " last="
                    + lastTimestampFramePosition
                    + " framesWritten="
                    + framesWritten);
          } else if (nowMs - lastTimestampLogRealtimeMs >= 500) {
            Log.i(
                TAG,
                "offload timestamp: raw="
                    + rawFramePosition
                    + " extrapolated="
                    + (rawFramePosition + extrapolatedFrames)
                    + " framesWritten="
                    + framesWritten
                    + " tsNs="
                    + audioTimestamp.nanoTime);
            lastTimestampLogRealtimeMs = nowMs;
          }
          lastTimestampFramePosition = rawFramePosition;
        }
        return Math.max(audioTimestamp.framePosition + extrapolatedFrames, 0);
      }
    } catch (Exception e) {
      // getTimestamp can throw on some devices; fall through.
    }
    // Fallback for PCM tracks or early playback before first timestamp.
    if (isCompressed) {
      long nowMs = SystemClock.elapsedRealtime();
      if (nowMs - lastTimestampLogRealtimeMs >= 500) {
        Log.w(
            TAG,
            "offload timestamp unavailable, using playback head fallback"
                + " framesWritten="
                + framesWritten
                + " playbackHead="
                + (audioTrack.getPlaybackHeadPosition() & 0xFFFFFFFFL));
        lastTimestampLogRealtimeMs = nowMs;
      }
    }
    return audioTrack.getPlaybackHeadPosition() & 0xFFFFFFFFL;
  }

  /**
   * Returns the total hardware output latency in microseconds via AudioTrack.getLatency(). This
   * includes the HAL pipeline and physical output buffer depth, making it more reliable than frame
   * counting for passthrough (compressed) formats where the HAL may report write-pointer advances
   * rather than true play-pointer advances.
   *
   * <p>AudioTrack.getLatency() has been present in Android since API 1 (though marked @hide in
   * documentation). We access it reflectively so that the build does not depend on the hidden API
   * directly.
   */
  private long getHardwareLatencyUs() {
    if (audioTrack == null) {
      return 100_000L; // 100 ms safe default
    }
    try {
      java.lang.reflect.Method m = AudioTrack.class.getDeclaredMethod("getLatency");
      m.setAccessible(true);
      int latencyMs = (Integer) m.invoke(audioTrack);
      if (latencyMs > 0 && latencyMs < 2000) { // sanity: 0–2 s
        return (long) latencyMs * 1000L;
      }
    } catch (Exception e) {
      // Reflection failed or value out of range — use fallback.
    }
    return 100_000L; // 100 ms fallback
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

  private static int channelCountToMask(int channelCount) {
        switch (channelCount) {
            case 1:
                return AudioFormat.CHANNEL_OUT_MONO;
            case 2:
                return AudioFormat.CHANNEL_OUT_STEREO;
            case 4:
                return AudioFormat.CHANNEL_OUT_QUAD;
            case 6:
                return AudioFormat.CHANNEL_OUT_5POINT1;
            case 8:
                return AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
            default:
                return (1 << channelCount) - 1;
        }
    }

    private static int bytesPerSampleForEncoding(int androidEncoding) {
        switch (androidEncoding) {
            case AudioFormat.ENCODING_PCM_8BIT:
                return 1;
            case AudioFormat.ENCODING_PCM_16BIT:
                return 2;
      case AudioFormat.ENCODING_PCM_FLOAT:
      case AudioFormat.ENCODING_PCM_32BIT:
        return 4;
      default:
        return 2;
    }
  }

  private static boolean isCompressedEncoding(int avpEncoding) {
    return avpEncoding >= AudioSink.ENCODING_AC3;
  }

  /**
   * Returns the number of PCM samples per compressed access unit for the given AVP encoding
   * constant. Used to convert access-unit counts into PCM-equivalent frame counts so that
   * framesWritten and getTimestamp() positions are in the same unit.
   */
  private static int getSamplesPerFrame(int avpEncoding) {
    switch (avpEncoding) {
      case AudioSink.ENCODING_AAC_LC:
        return 1024; // AAC-LC: 1024 samples per frame
      case AudioSink.ENCODING_AC3:
        return 1536; // AC-3: 256 samples × 6 blocks
      case AudioSink.ENCODING_E_AC3:
      case AudioSink.ENCODING_E_AC3_JOC:
        return 1536; // E-AC-3: same as AC-3 per syncframe
      case AudioSink.ENCODING_DTS:
        return 512; // DTS: typically 512 samples per frame
      case AudioSink.ENCODING_DTS_HD:
        return 512;
      case AudioSink.ENCODING_DOLBY_TRUEHD:
        return 40; // TrueHD: 40 samples per frame (varies)
      case AudioSink.ENCODING_AC4:
        return 2048; // AC-4: typically 2048 samples per frame
      default:
        return 1024; // safe default
    }
  }
}
