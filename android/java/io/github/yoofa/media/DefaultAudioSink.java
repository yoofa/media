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
    private static final int TIMESTAMP_STATE_INITIALIZING = 0;
    private static final int TIMESTAMP_STATE_TIMESTAMP = 1;
    private static final int TIMESTAMP_STATE_TIMESTAMP_ADVANCING = 2;
    private static final int TIMESTAMP_STATE_NO_TIMESTAMP = 3;
    private static final int TIMESTAMP_STATE_ERROR = 4;
    private static final long FAST_TIMESTAMP_SAMPLE_INTERVAL_US = 10_000;
    private static final long SLOW_TIMESTAMP_SAMPLE_INTERVAL_US = 10_000_000;
    private static final long ERROR_TIMESTAMP_SAMPLE_INTERVAL_US = 500_000;
    private static final long INITIALIZING_DURATION_US = 500_000;
    private static final long WAIT_FOR_ADVANCING_TIMESTAMP_US = 2_000_000;
    private static final long MAX_ADVANCING_TIMESTAMP_DRIFT_US = 1_000;
    private static final long MAX_AUDIO_TIMESTAMP_OFFSET_US = 5_000_000;
    private static final long PLAYHEAD_OFFSET_SAMPLE_INTERVAL_US = 30_000;
    private static final long RAW_PLAYHEAD_SAMPLE_INTERVAL_MS = 5;
    private static final int MAX_PLAYHEAD_OFFSET_COUNT = 10;

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
    private long lastRawTimestampFramePosition = -1;
    private long timestampWrapCount;
    private long lastTimestampLogRealtimeMs;
    private int timestampState;
    private long initializeSystemTimeUs;
    private long lastTimestampSampleTimeUs;
    private long timestampSampleIntervalUs;
    private long initialTimestampPositionFrames;
    private long initialTimestampSystemTimeUs;
    private final long[] playheadOffsetsUs = new long[MAX_PLAYHEAD_OFFSET_COUNT];
    private int nextPlayheadOffsetIndex;
    private int playheadOffsetCount;
    private long smoothedPlayheadOffsetUs;
    private long lastPlayheadSampleTimeUs;
    private long rawPlaybackHeadPosition;
    private long rawPlaybackHeadWrapCount;
    private long lastRawPlaybackHeadSampleTimeMs;

    @Override
    public boolean open(int sampleRate, int channelCount, int encoding) {
        Log.i(
                TAG,
                "open: sampleRate="
                        + sampleRate
                        + " channels="
                        + channelCount
                        + " encoding="
                        + encoding);
        close();

        this.sampleRate = sampleRate;
        this.channelCount = channelCount;
        this.framesWritten = 0;
        this.isCompressed = isCompressedEncoding(encoding);
        resetPositionTracker();

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
            if (offloaded) {
                configureOffloadStartThreshold(audioTrack);
            }
            Log.i(
                    TAG,
                    "AudioTrack created: bufferSize="
                            + bufferSize
                            + " minBufferSize="
                            + minBufferSize
                            + " sampleRate="
                            + audioTrack.getSampleRate()
                            + " startThresholdFrames="
                            + getOffloadStartThresholdFrames(audioTrack)
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
    public int write(ByteBuffer data, int size, int frameCount) {
        if (audioTrack == null) {
            return -1;
        }
        if (isCompressed) {
            int startPosition = data.position();
            int written = audioTrack.write(data, size, AudioTrack.WRITE_NON_BLOCKING);
            data.position(startPosition);
            if (written == size) {
                // Count the full PCM-equivalent duration represented by this compressed
                // write only after AudioTrack has accepted the whole buffer.
                framesWritten += frameCount > 0 ? frameCount : samplesPerFrame;
            } else if (written > 0) {
                Log.w(
                        TAG,
                        "write incomplete: requested="
                                + size
                                + " written="
                                + written
                                + " compressed=true");
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
            resetPositionTracker();
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
            resetPositionTracker();
        }
    }

    @Override
    public void flush() {
        if (audioTrack != null) {
            Log.i(TAG, "flush");
            audioTrack.flush();
            framesWritten = 0;
            resetPositionTracker();
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
            resetPositionTracker();
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
        resetPositionTracker();
    }

    @Override
    public boolean isReady() {
        return audioTrack != null && audioTrack.getState() == AudioTrack.STATE_INITIALIZED;
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
     * getPlaybackHeadPosition() is unreliable), with a fallback to getPlaybackHeadPosition() for
     * PCM when getTimestamp() is unavailable.
     */
    private long getPlayedFrameCount() {
        if (audioTrack == null) {
            return 0;
        }
        long systemTimeUs = System.nanoTime() / 1000;
        maybeSamplePlayheadOffset(systemTimeUs);
        maybePollTimestamp(systemTimeUs);

        long playbackHeadFrames = getPlaybackHeadPosition();
        long playedFrames =
                hasAdvancingTimestamp()
                        ? getTimestampPositionFrames(systemTimeUs)
                        : getPlaybackHeadPositionEstimateFrames(systemTimeUs);

        // Cap played frames at what we have written. The timestamp API may
        // extrapolate forward from a stale sample, producing values beyond the
        // actual content length (especially on offload tracks where the HAL
        // updates the timestamp infrequently).
        if (framesWritten > 0 && playedFrames > framesWritten) {
            playedFrames = framesWritten;
        }

        if (isCompressed) {
            long nowMs = SystemClock.elapsedRealtime();
            if (nowMs - lastTimestampLogRealtimeMs >= 500) {
                Log.i(
                        TAG,
                        "offload position:"
                                + " source="
                                + (hasAdvancingTimestamp() ? "timestamp" : "playhead")
                                + " played="
                                + playedFrames
                                + " playbackHead="
                                + playbackHeadFrames
                                + " timestamp="
                                + lastTimestampFramePosition
                                + " framesWritten="
                                + framesWritten
                                + " tsNs="
                                + audioTimestamp.nanoTime);
                lastTimestampLogRealtimeMs = nowMs;
            }
        }

        return Math.max(playedFrames, 0);
    }

    private void resetPositionTracker() {
        lastTimestampFramePosition = -1;
        lastRawTimestampFramePosition = -1;
        timestampWrapCount = 0;
        lastTimestampLogRealtimeMs = 0;
        rawPlaybackHeadPosition = 0;
        rawPlaybackHeadWrapCount = 0;
        lastRawPlaybackHeadSampleTimeMs = 0;
        smoothedPlayheadOffsetUs = 0;
        lastPlayheadSampleTimeUs = 0;
        nextPlayheadOffsetIndex = 0;
        playheadOffsetCount = 0;
        initialTimestampPositionFrames = -1;
        initialTimestampSystemTimeUs = 0;
        updateTimestampState(TIMESTAMP_STATE_INITIALIZING);
    }

    private void maybeSamplePlayheadOffset(long systemTimeUs) {
        if (sampleRate <= 0
                || systemTimeUs - lastPlayheadSampleTimeUs < PLAYHEAD_OFFSET_SAMPLE_INTERVAL_US) {
            return;
        }

        long playbackHeadPositionUs = getPlaybackHeadPositionUs();
        if (playbackHeadPositionUs == 0) {
            return;
        }

        playheadOffsetsUs[nextPlayheadOffsetIndex] = playbackHeadPositionUs - systemTimeUs;
        nextPlayheadOffsetIndex = (nextPlayheadOffsetIndex + 1) % MAX_PLAYHEAD_OFFSET_COUNT;
        if (playheadOffsetCount < MAX_PLAYHEAD_OFFSET_COUNT) {
            playheadOffsetCount++;
        }
        smoothedPlayheadOffsetUs = 0;
        for (int i = 0; i < playheadOffsetCount; i++) {
            smoothedPlayheadOffsetUs += playheadOffsetsUs[i] / playheadOffsetCount;
        }
        lastPlayheadSampleTimeUs = systemTimeUs;
    }

    private void maybePollTimestamp(long systemTimeUs) {
        if (audioTrack == null || sampleRate <= 0) {
            return;
        }
        if ((systemTimeUs - lastTimestampSampleTimeUs) < timestampSampleIntervalUs) {
            return;
        }

        lastTimestampSampleTimeUs = systemTimeUs;
        long playbackHeadPositionEstimateUs = getPlaybackHeadPositionEstimateUs(systemTimeUs);
        boolean updatedTimestamp = false;
        try {
            updatedTimestamp = audioTrack.getTimestamp(audioTimestamp);
        } catch (Exception e) {
            updatedTimestamp = false;
        }

        if (updatedTimestamp) {
            updateTimestampFramePosition();
            checkTimestampIsPlausibleAndUpdateState(systemTimeUs, playbackHeadPositionEstimateUs);
        }

        switch (timestampState) {
            case TIMESTAMP_STATE_INITIALIZING:
                if (updatedTimestamp) {
                    long timestampSystemTimeUs = audioTimestamp.nanoTime / 1000;
                    if (timestampSystemTimeUs >= initializeSystemTimeUs) {
                        initialTimestampPositionFrames = lastTimestampFramePosition;
                        initialTimestampSystemTimeUs = timestampSystemTimeUs;
                        updateTimestampState(TIMESTAMP_STATE_TIMESTAMP);
                    }
                } else if (systemTimeUs - initializeSystemTimeUs > INITIALIZING_DURATION_US) {
                    updateTimestampState(TIMESTAMP_STATE_NO_TIMESTAMP);
                }
                break;
            case TIMESTAMP_STATE_TIMESTAMP:
                if (!updatedTimestamp) {
                    // Timestamp stopped (e.g. offload EOS). Fall back to playback
                    // head position rather than wiping all position state.
                    updateTimestampState(TIMESTAMP_STATE_NO_TIMESTAMP);
                    break;
                }
                if (isTimestampAdvancing(systemTimeUs)) {
                    updateTimestampState(TIMESTAMP_STATE_TIMESTAMP_ADVANCING);
                } else if (systemTimeUs - initializeSystemTimeUs
                        > WAIT_FOR_ADVANCING_TIMESTAMP_US) {
                    updateTimestampState(TIMESTAMP_STATE_NO_TIMESTAMP);
                } else {
                    initialTimestampPositionFrames = lastTimestampFramePosition;
                    initialTimestampSystemTimeUs = audioTimestamp.nanoTime / 1000;
                }
                break;
            case TIMESTAMP_STATE_TIMESTAMP_ADVANCING:
                if (!updatedTimestamp) {
                    // Timestamp stopped — most likely the offload track finished draining.
                    // Fall back to playback head position extrapolation instead of wiping
                    // all position state (which would erroneously return played=0).
                    updateTimestampState(TIMESTAMP_STATE_NO_TIMESTAMP);
                }
                break;
            case TIMESTAMP_STATE_NO_TIMESTAMP:
                if (updatedTimestamp) {
                    resetPositionTracker();
                }
                break;
            case TIMESTAMP_STATE_ERROR:
                break;
            default:
                break;
        }
    }

    private void updateTimestampState(int state) {
        timestampState = state;
        switch (state) {
            case TIMESTAMP_STATE_INITIALIZING:
                initializeSystemTimeUs = System.nanoTime() / 1000;
                lastTimestampSampleTimeUs = 0;
                timestampSampleIntervalUs = FAST_TIMESTAMP_SAMPLE_INTERVAL_US;
                initialTimestampPositionFrames = -1;
                initialTimestampSystemTimeUs = 0;
                break;
            case TIMESTAMP_STATE_TIMESTAMP:
                timestampSampleIntervalUs = FAST_TIMESTAMP_SAMPLE_INTERVAL_US;
                break;
            case TIMESTAMP_STATE_TIMESTAMP_ADVANCING:
            case TIMESTAMP_STATE_NO_TIMESTAMP:
                timestampSampleIntervalUs = SLOW_TIMESTAMP_SAMPLE_INTERVAL_US;
                break;
            case TIMESTAMP_STATE_ERROR:
                timestampSampleIntervalUs = ERROR_TIMESTAMP_SAMPLE_INTERVAL_US;
                break;
            default:
                break;
        }
    }

    private void checkTimestampIsPlausibleAndUpdateState(
            long systemTimeUs, long playbackHeadPositionEstimateUs) {
        long timestampSystemTimeUs = audioTimestamp.nanoTime / 1000;
        long timestampPositionUs =
                computeTimestampPositionUs(
                        lastTimestampFramePosition, timestampSystemTimeUs, systemTimeUs);

        if (Math.abs(timestampSystemTimeUs - systemTimeUs) > MAX_AUDIO_TIMESTAMP_OFFSET_US) {
            Log.w(
                    TAG,
                    "Rejecting audio timestamp with stale system time:"
                            + " tsUs="
                            + timestampSystemTimeUs
                            + " nowUs="
                            + systemTimeUs);
            updateTimestampState(TIMESTAMP_STATE_ERROR);
        } else if (Math.abs(timestampPositionUs - playbackHeadPositionEstimateUs)
                > MAX_AUDIO_TIMESTAMP_OFFSET_US) {
            Log.w(
                    TAG,
                    "Rejecting audio timestamp with implausible position:"
                            + " tsPositionUs="
                            + timestampPositionUs
                            + " playbackHeadEstimateUs="
                            + playbackHeadPositionEstimateUs);
            updateTimestampState(TIMESTAMP_STATE_ERROR);
        } else if (timestampState == TIMESTAMP_STATE_ERROR) {
            resetPositionTracker();
        }
    }

    private void updateTimestampFramePosition() {
        long rawFramePosition = audioTimestamp.framePosition & 0xFFFFFFFFL;
        if (lastRawTimestampFramePosition >= 0
                && rawFramePosition < lastRawTimestampFramePosition) {
            timestampWrapCount++;
        }
        lastRawTimestampFramePosition = rawFramePosition;
        lastTimestampFramePosition = rawFramePosition + (timestampWrapCount << 32);
    }

    private boolean isTimestampAdvancing(long systemTimeUs) {
        if (lastTimestampFramePosition <= initialTimestampPositionFrames) {
            return false;
        }
        long positionEstimateUsingInitialTimestampUs =
                computeTimestampPositionUs(
                        initialTimestampPositionFrames, initialTimestampSystemTimeUs, systemTimeUs);
        long positionEstimateUsingCurrentTimestampUs =
                computeTimestampPositionUs(
                        lastTimestampFramePosition, audioTimestamp.nanoTime / 1000, systemTimeUs);
        return Math.abs(
                        positionEstimateUsingCurrentTimestampUs
                                - positionEstimateUsingInitialTimestampUs)
                < MAX_ADVANCING_TIMESTAMP_DRIFT_US;
    }

    private boolean hasAdvancingTimestamp() {
        return timestampState == TIMESTAMP_STATE_TIMESTAMP_ADVANCING;
    }

    private long getTimestampPositionFrames(long systemTimeUs) {
        if (sampleRate <= 0 || lastTimestampFramePosition < 0) {
            return 0;
        }
        long positionUs =
                computeTimestampPositionUs(
                        lastTimestampFramePosition, audioTimestamp.nanoTime / 1000, systemTimeUs);
        return positionUs * sampleRate / 1_000_000L;
    }

    private long computeTimestampPositionUs(
            long timestampPositionFrames, long timestampSystemTimeUs, long systemTimeUs) {
        long timestampPositionUs = timestampPositionFrames * 1_000_000L / sampleRate;
        long elapsedSinceTimestampUs = systemTimeUs - timestampSystemTimeUs;
        if (elapsedSinceTimestampUs < 0) {
            elapsedSinceTimestampUs = 0;
        }
        return timestampPositionUs + elapsedSinceTimestampUs;
    }

    private long getPlaybackHeadPositionEstimateUs(long systemTimeUs) {
        if (sampleRate <= 0) {
            return 0;
        }
        long positionUs =
                playheadOffsetCount == 0
                        ? getPlaybackHeadPositionUs()
                        : systemTimeUs + smoothedPlayheadOffsetUs;
        if (positionUs < 0) {
            return 0;
        }
        return positionUs;
    }

    private long getPlaybackHeadPositionEstimateFrames(long systemTimeUs) {
        return getPlaybackHeadPositionEstimateUs(systemTimeUs) * sampleRate / 1_000_000L;
    }

    private long getPlaybackHeadPositionUs() {
        return getPlaybackHeadPosition() * 1_000_000L / sampleRate;
    }

    private long getPlaybackHeadPosition() {
        if (audioTrack == null) {
            return 0;
        }
        long currentTimeMs = SystemClock.elapsedRealtime();
        if ((currentTimeMs - lastRawPlaybackHeadSampleTimeMs) >= RAW_PLAYHEAD_SAMPLE_INTERVAL_MS) {
            updateRawPlaybackHeadPosition();
            lastRawPlaybackHeadSampleTimeMs = currentTimeMs;
        }
        return rawPlaybackHeadPosition + (rawPlaybackHeadWrapCount << 32);
    }

    private void updateRawPlaybackHeadPosition() {
        if (audioTrack == null || audioTrack.getPlayState() == AudioTrack.PLAYSTATE_STOPPED) {
            return;
        }
        long playbackHeadPosition = audioTrack.getPlaybackHeadPosition() & 0xFFFFFFFFL;
        if (rawPlaybackHeadPosition > playbackHeadPosition) {
            // Only count as a 32-bit counter wrap if the decrease is larger than half the
            // 32-bit range (~27 hours of audio at 44100 Hz). A smaller decrease indicates
            // a position reset (e.g. offload EOS drain) rather than a genuine overflow.
            if (rawPlaybackHeadPosition - playbackHeadPosition >= 0x80000000L) {
                rawPlaybackHeadWrapCount++;
            }
            // else: HAL position reset — do not increment wrap count
        }
        rawPlaybackHeadPosition = playbackHeadPosition;
    }

    private void configureOffloadStartThreshold(AudioTrack audioTrack) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S || samplesPerFrame <= 0) {
            return;
        }
        int desiredThresholdFrames = 1;
        int currentThresholdFrames = getOffloadStartThresholdFrames(audioTrack);
        if (currentThresholdFrames > 0 && currentThresholdFrames <= desiredThresholdFrames) {
            return;
        }
        try {
            audioTrack.setStartThresholdInFrames(desiredThresholdFrames);
            Log.i(
                    TAG,
                    "Configured offload start threshold:"
                            + " previous="
                            + currentThresholdFrames
                            + " desired="
                            + desiredThresholdFrames
                            + " actual="
                            + getOffloadStartThresholdFrames(audioTrack));
        } catch (Exception e) {
            Log.w(TAG, "Failed to configure offload start threshold", e);
        }
    }

    private int getOffloadStartThresholdFrames(AudioTrack audioTrack) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return -1;
        }
        try {
            return audioTrack.getStartThresholdInFrames();
        } catch (Exception e) {
            return -1;
        }
    }

    /**
     * Returns the total hardware output latency in microseconds via AudioTrack.getLatency(). This
     * includes the HAL pipeline and physical output buffer depth, making it more reliable than
     * frame counting for passthrough (compressed) formats where the HAL may report write-pointer
     * advances rather than true play-pointer advances.
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
