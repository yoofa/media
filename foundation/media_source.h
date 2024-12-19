/*
 * media_source.h
 * Copyright (C) 2022 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_SOURCE_H
#define MEDIA_SOURCE_H

#include <memory>
#include <vector>

#include "base/constructor_magic.h"

#include "media_errors.h"
#include "media_format.h"
#include "media_packet.h"
#include "media_source_base.h"
#include "message_object.h"

namespace ave {
namespace media {

class MediaSource : public MessageObject, public MediaPacketSource {
 public:
  // Options that modify read() behaviour. The default is to
  // a) not request a seek
  // b) not be late, i.e. lateness_us = 0
  struct ReadOptions {
    enum SeekMode : int32_t {
      SEEK_PREVIOUS_SYNC,
      SEEK_NEXT_SYNC,
      SEEK_CLOSEST_SYNC,
      SEEK_CLOSEST,
    };

    ReadOptions();

    // Reset everything back to defaults.
    void Reset();

    void SetSeekTo(int64_t time_us, SeekMode mode = SEEK_CLOSEST_SYNC);
    void ClearSeekTo();
    bool GetSeekTo(int64_t* time_us, SeekMode* mode) const;

    // TODO: remove this if unused.
    void SetLateBy(int64_t lateness_us);
    int64_t GetLateBy() const;

    void SetNonBlocking();
    void ClearNonBlocking();
    bool GetNonBlocking() const;

    // Used to clear all non-persistent options for multiple buffer reads.
    void ClearNonPersistent() { ClearSeekTo(); }

   private:
    enum Options {
      kSeekTo_Option = 1,
    };

    uint32_t options_;
    int64_t seek_time_us_;
    SeekMode seek_mode_;
    int64_t lateness_us_;
    bool non_blocking_;
  } __attribute__((packed));  // sent through Binder

  MediaSource() = default;
  ~MediaSource() override = default;

  // To be called before any other methods on this object, except
  // getFormat().
  virtual status_t Start(std::shared_ptr<Message> params) = 0;

  // Any blocking read call returns immediately with a result of NO_INIT.
  // It is an error to call any methods other than start after this call
  // returns. Any buffers the object may be holding onto at the time of
  // the stop() call are released.
  // Also, it is imperative that any buffers output by this object and
  // held onto by callers be released before a call to stop() !!!
  virtual status_t Stop() = 0;

  // Returns the format of the data output by this media source.
  virtual std::shared_ptr<MediaFormat> GetFormat() = 0;

  virtual bool SupportReadMultiple() { return false; }

  // Returns a new buffer of data. Call blocks until a
  // buffer is available, an error is encountered of the end of the stream
  // is reached.
  // End of stream is signalled by a result of ERROR_END_OF_STREAM.
  // A result of INFO_FORMAT_CHANGED indicates that the format of this
  // MediaSource has changed mid-stream, the client can continue reading
  // but should be prepared for buffers of the new configuration.
  virtual status_t Read(std::shared_ptr<MediaPacket>& packet,
                        const ReadOptions* options) = 0;

  virtual status_t ReadMultiple(
      std::vector<std::shared_ptr<MediaPacket>>& /* packets */,
      size_t /* count */,
      const ReadOptions* /* options */) {
    return ERROR_UNSUPPORTED;
  }

  // Causes this source to suspend pulling data from its upstream source
  // until a subsequent read-with-seek. This is currently not supported
  // as such by any source. E.g. MediaCodecSource does not suspend its
  // upstream source, and instead discard upstream data while paused.
  virtual status_t Pause() { return ERROR_UNSUPPORTED; }

  // The consumer of this media source requests that the given buffers
  // are to be returned exclusively in response to read calls.
  // This will be called after a successful start() and before the
  // first read() call.
  // Callee assumes ownership of the buffers if no error is returned.
  virtual status_t SetBuffers(
      const std::vector<std::shared_ptr<MediaPacket>>& /* buffers */) {
    return ERROR_UNSUPPORTED;
  }

  // The consumer of this media source requests the source stops sending
  // buffers with timestamp larger than or equal to stopTimeUs. stopTimeUs
  // must be in the same time base as the startTime passed in start(). If
  // source does not support this request, ERROR_UNSUPPORTED will be returned.
  // If stopTimeUs is invalid, BAD_VALUE will be returned. This could be
  // called at any time even before source starts and it could be called
  // multiple times. Setting stopTimeUs to be -1 will effectively cancel the
  // stopTimeUs set previously. If stopTimeUs is larger than or equal to last
  // buffer's timestamp, source will start to drop buffer when it gets a buffer
  // with timestamp larger than or equal to stopTimeUs. If stopTimeUs is smaller
  // than or equal to last buffer's timestamp, source will drop all the incoming
  // buffers immediately. After setting stopTimeUs, source may still stop
  // sending buffers with timestamp less than stopTimeUs if it is stopped by the
  // consumer.
  virtual status_t SetStopTimeUs(int64_t /* stopTimeUs */) {
    return ERROR_UNSUPPORTED;
  }

 private:
  /* data */

  AVE_DISALLOW_COPY_AND_ASSIGN(MediaSource);
};

using ReadOptions = MediaSource::ReadOptions;

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_SOURCE_H */
