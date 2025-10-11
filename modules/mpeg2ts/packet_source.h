/*
 * packet_source.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 *
 * Ported from Android MPEG2-TS module (AnotherPacketSource)
 * Original Copyright (C) 2010 The Android Open Source Project
 */

#ifndef MODULES_MPEG2TS_PACKET_SOURCE_H
#define MODULES_MPEG2TS_PACKET_SOURCE_H

#include <list>
#include <memory>
#include <mutex>

#include "base/constructor_magic.h"
#include "foundation/media_errors.h"
#include "foundation/media_source.h"
#include "foundation/message.h"

namespace ave {
namespace media {

class Buffer;
class MediaMeta;

namespace mpeg2ts {

enum class DiscontinuityType {
  NONE = 0,
  TIME = 1,
  AUDIO_FORMAT = 2,
  VIDEO_FORMAT = 4,
  ABSOLUTE_TIME = 8,
  TIME_OFFSET = 16,

  // For legacy reasons this also implies a time discontinuity.
  FORMATCHANGE = AUDIO_FORMAT | VIDEO_FORMAT | TIME,
  FORMAT_ONLY = AUDIO_FORMAT | VIDEO_FORMAT,
};

class PacketSource : public MediaSource {
 public:
  explicit PacketSource(std::shared_ptr<MediaMeta> meta);
  virtual ~PacketSource();

  void SetFormat(std::shared_ptr<MediaMeta> meta);

  status_t Start(std::shared_ptr<Message> params) override;
  status_t Stop() override;
  std::shared_ptr<MediaMeta> GetFormat() override;

  status_t Read(std::shared_ptr<MediaFrame>& frame,
                const ReadOptions* options) override;

  void Clear();

  // Returns true if we have any packets including discontinuities
  bool HasBufferAvailable(status_t* final_result);

  // Returns true if we have packets that's not discontinuities
  bool HasDataBufferAvailable(status_t* final_result);

  // Returns the number of available buffers.
  size_t GetAvailableBufferCount(status_t* final_result);

  // Returns the difference between the last and the first queued
  // presentation timestamps since the last discontinuity (if any).
  int64_t GetBufferedDurationUs(status_t* final_result);

  status_t NextBufferTime(int64_t* time_us);

  void QueueAccessUnit(std::shared_ptr<Buffer> buffer);

  void QueueDiscontinuity(DiscontinuityType type,
                          std::shared_ptr<Message> extra,
                          bool discard);

  void SignalEOS(status_t result);

  status_t DequeueAccessUnit(std::shared_ptr<Buffer>& buffer);

  bool IsFinished(int64_t duration) const;

  void Enable(bool enable);

  std::shared_ptr<Message> GetLatestEnqueuedMeta();
  std::shared_ptr<Message> GetLatestDequeuedMeta();

 private:
  struct DiscontinuitySegment {
    int64_t max_deque_time_us_;
    int64_t max_enque_time_us_;

    DiscontinuitySegment()
        : max_deque_time_us_(-1), max_enque_time_us_(-1) {}

    void Clear() { max_deque_time_us_ = max_enque_time_us_ = -1; }
  };

  std::mutex lock_;
  std::condition_variable condition_;

  bool is_audio_;
  bool is_video_;
  bool enabled_;
  std::shared_ptr<MediaMeta> format_;
  int64_t last_queued_time_us_;
  std::list<std::shared_ptr<Buffer>> buffers_;
  status_t eos_result_;
  std::shared_ptr<Message> latest_enqueued_meta_;
  std::shared_ptr<Message> latest_dequeued_meta_;

  std::list<DiscontinuitySegment> discontinuity_segments_;

  bool WasFormatChange(int32_t discontinuity_type) const;

  AVE_DISALLOW_COPY_AND_ASSIGN(PacketSource);
};

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave

#endif  // MODULES_MPEG2TS_PACKET_SOURCE_H
