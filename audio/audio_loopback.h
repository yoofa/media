/*
 * audio_loopback.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_LOOPBACK_H_H_
#define AVE_MEDIA_AUDIO_LOOPBACK_H_H_

#include <functional>
#include "audio.h"
#include "base/types.h"

namespace ave {
namespace media {

class AudioLoopback {
 public:
  enum cb_event_t {
    CB_EVENT_MORE_DATA,      // New data available
    CB_EVENT_OVERRUN,        // Buffer overrun occurred
    CB_EVENT_DEVICE_CHANGE,  // Audio device changed
    CB_EVENT_TEAR_DOWN       // The AudioLoopback was invalidated
  };

  using AudioCallback = std::function<void(AudioLoopback* audio_loopback,
                                           void* buffer,
                                           size_t size,
                                           void* cookie,
                                           cb_event_t event)>;

  virtual ~AudioLoopback() = default;

  virtual bool ready() const = 0;
  virtual ssize_t bufferSize() const = 0;
  virtual ssize_t frameCount() const = 0;
  virtual ssize_t channelCount() const = 0;
  virtual ssize_t frameSize() const = 0;
  virtual uint32_t sampleRate() const = 0;
  virtual uint32_t latency() const = 0;

  // Open loopback capture with specific config
  virtual status_t Open(audio_config_t config,
                        AudioCallback cb,
                        void* cookie) = 0;

  virtual status_t Start() = 0;
  virtual void Stop() = 0;
  virtual void Flush() = 0;
  virtual void Close() = 0;

  // Get available audio sources for loopback
  virtual std::vector<std::string> GetAvailableSources() const = 0;

  // Select specific audio source
  virtual status_t SelectSource(const std::string& source_id) = 0;

  // Optional: volume monitoring
  virtual float GetPeakLevel() const = 0;
  virtual float GetRMSLevel() const = 0;
};

}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_AUDIO_LOOPBACK_H_H_ */
