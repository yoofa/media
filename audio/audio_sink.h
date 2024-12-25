/*
 * audio_sink.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AUDIO_SINK_H
#define AUDIO_SINK_H

#include <functional>

#include "base/types.h"

namespace ave {
namespace media {

// porting from android MediaPlayerInterface.h
class AudioSink {
 public:
  enum cb_event_t {
    CB_EVENT_FILL_BUFFER,  // Request to write more data to buffer.
    CB_EVENT_STREAM_END,   // Sent after all the buffers queued in AF and HW are
                           // played back (after stop is called)
    CB_EVENT_TEAR_DOWN     // The AudioTrack was invalidated due to use case
                           // change: Need to re-evaluate offloading options
  };
  using AudioCallback = std::function<void(AudioSink* audio_sink,
                                           void* buffer,
                                           size_t size,
                                           void* cookie,
                                           cb_event_t event)>;
  virtual ~AudioSink() = default;

  virtual bool ready() const = 0;  // audio output is open and ready
  virtual ssize_t bufferSize() const = 0;
  virtual ssize_t frameCount() const = 0;
  virtual ssize_t channelCount() const = 0;
  virtual ssize_t frameSize() const = 0;
  virtual uint32_t latency() const = 0;
  virtual float msecsPerFrame() const = 0;

  virtual status_t GetPosition(uint32_t* position) const = 0;
  // virtual status_t getTimestamp(AudioTimestamp& ts) const = 0;
  virtual int64_t GetPlayedOutDurationUs(int64_t nowUs) const = 0;
  virtual status_t GetFramesWritten(uint32_t* frameswritten) const = 0;
  // virtual audio_session_t getSessionId() const = 0;
  // virtual audio_stream_type_t getAudioStreamType() const = 0;
  virtual uint32_t GetSampleRate() const = 0;
  virtual int64_t GetBufferDurationInUs() const = 0;
};

}  // namespace media
}  // namespace ave
#endif /* !AUDIO_SINK_H */
