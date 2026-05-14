/*
 * audio_track.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AUDIO_TRACK_H
#define AUDIO_TRACK_H

#include <functional>

#include "audio.h"
#include "base/types.h"

namespace ave {
namespace media {
class AudioTrack {
 public:
  enum cb_event_t {
    CB_EVENT_FILL_BUFFER,  // Request to write more data to buffer.
    CB_EVENT_STREAM_END,   // Sent after all the buffers queued in AF and HW are
                           // played back (after stop is called)
    CB_EVENT_TEAR_DOWN     // The AudioTrack was invalidated due to use case
                           // change: Need to re-evaluate offloading options
  };

  using AudioCallback = std::function<void(AudioTrack* audio_track,
                                           void* buffer,
                                           size_t size,
                                           void* cookie,
                                           cb_event_t event)>;

  virtual ~AudioTrack() = default;

  virtual bool ready() const = 0;  // audio output is open and ready
  virtual ssize_t bufferSize() const = 0;
  virtual ssize_t frameCount() const = 0;
  virtual ssize_t channelCount() const = 0;
  virtual ssize_t frameSize() const = 0;
  virtual uint32_t sampleRate() const = 0;
  virtual uint32_t latency() const = 0;
  virtual float msecsPerFrame() const = 0;

  virtual status_t GetPosition(uint32_t* position) const = 0;
  // virtual status_t getTimestamp(AudioTimestamp& ts) const = 0;
  virtual int64_t GetPlayedOutDurationUs(int64_t nowUs) const = 0;
  virtual status_t GetFramesWritten(uint32_t* frameswritten) const = 0;
  virtual int64_t GetBufferDurationInUs() const = 0;

  // if cb is nullptr, use write() to fill the buffer
  virtual status_t Open(audio_config_t config,
                        AudioCallback cb,
                        void* cookie) = 0;

  status_t Open(audio_config_t config) {
    return Open(config, nullptr, nullptr);
  }

  /* Input parameter |size| is in byte units stored in |buffer|.
   * Data is copied over and actual number of bytes written (>= 0)
   * is returned, or no data is copied and a negative status code
   * is returned (even when |blocking| is true).
   * When |blocking| is false, AudioSink will immediately return after
   * part of or full |buffer| is copied over.
   * When |blocking| is true, AudioSink will wait to copy the entire
   * buffer, unless an error occurs or the copy operation is
   * prematurely stopped.
   */
  virtual ssize_t Write(const void* buffer, size_t size, bool blocking) = 0;

  // Write() with blocking
  ssize_t Write(const void* buffer, size_t size) {
    return Write(buffer, size, true);
  }

  virtual status_t Start() = 0;
  virtual void Stop() = 0;
  virtual void Flush() = 0;
  virtual void Pause() = 0;
  virtual void Close() = 0;
};
}  // namespace media
}  // namespace ave

#endif /* !AUDIO_TRACK_H */
