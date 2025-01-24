/*
 * audio_record.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AUDIO_RECORD_H
#define AUDIO_RECORD_H

#include <functional>
#include "audio.h"
#include "base/types.h"

namespace ave {
namespace media {
class AudioRecord {
 public:
  enum cb_event_t {
    CB_EVENT_MORE_DATA,  // Request to read more data from audio input
    CB_EVENT_OVERRUN,    // Buffer overrun occurred
    CB_EVENT_TEAR_DOWN   // The AudioRecord was invalidated
  };

  using AudioCallback = std::function<void(AudioRecord* audio_record,
                                           void* buffer,
                                           size_t size,
                                           void* cookie,
                                           cb_event_t event)>;

  virtual ~AudioRecord() = default;

  virtual bool ready() const = 0;
  virtual ssize_t bufferSize() const = 0;
  virtual ssize_t frameCount() const = 0;
  virtual ssize_t channelCount() const = 0;
  virtual ssize_t frameSize() const = 0;
  virtual uint32_t sampleRate() const = 0;
  virtual uint32_t latency() const = 0;

  virtual status_t GetPosition(uint32_t* position) const = 0;
  virtual int64_t GetRecordedDurationUs(int64_t nowUs) const = 0;
  virtual status_t GetFramesRead(uint32_t* frames_read) const = 0;

  // if cb is nullptr, use read() to get the buffer
  virtual status_t Open(audio_config_t config,
                        AudioCallback cb,
                        void* cookie) = 0;

  status_t Open(audio_config_t config) {
    return Open(config, nullptr, nullptr);
  }

  virtual ssize_t Read(void* buffer, size_t size, bool blocking) = 0;

  ssize_t Read(void* buffer, size_t size) { return Read(buffer, size, true); }

  virtual status_t Start() = 0;
  virtual void Stop() = 0;
  virtual void Flush() = 0;
  virtual void Pause() = 0;
  virtual void Close() = 0;

  // Optional: gain control
  virtual status_t SetGain(float gain) = 0;
  virtual float GetGain() const = 0;
};

}  // namespace media
}  // namespace ave

#endif /* !AUDIO_RECORD_H */
