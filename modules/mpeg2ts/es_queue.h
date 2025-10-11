/*
 * es_queue.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 *
 * Ported from Android MPEG2-TS module (ElementaryStreamQueue)
 * Original Copyright (C) 2010 The Android Open Source Project
 */

#ifndef MODULES_MPEG2TS_ES_QUEUE_H
#define MODULES_MPEG2TS_ES_QUEUE_H

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "base/constructor_magic.h"
#include "foundation/media_errors.h"

namespace ave {
namespace media {

class Buffer;
class MediaMeta;

namespace mpeg2ts {

class ESQueue {
 public:
  enum class Mode {
    INVALID = 0,
    H264,
    AAC,
    AC3,
    EAC3,
    AC4,
    MPEG_AUDIO,
    MPEG_VIDEO,
    MPEG4_VIDEO,
    PCM_AUDIO,
    METADATA,
    DTS,
    DTS_HD,
    DTS_UHD,
    HEVC,
  };

  enum Flags {
    // Data appended to the queue is always at access unit boundaries.
    kFlag_AlignedData = 1,
    kFlag_ScrambledData = 2,
    kFlag_SampleEncryptedData = 4,
  };

  explicit ESQueue(Mode mode, uint32_t flags = 0);
  ~ESQueue();

  status_t AppendData(const void* data,
                      size_t size,
                      int64_t time_us,
                      int32_t payload_offset = 0,
                      uint32_t pes_scrambling_control = 0);

  void SignalEOS();
  void Clear(bool clear_format);

  std::shared_ptr<Buffer> DequeueAccessUnit();

  std::shared_ptr<MediaMeta> GetFormat();

  bool IsScrambled() const;

  void SetCasInfo(int32_t system_id, const std::vector<uint8_t>& session_id);

 private:
  struct RangeInfo {
    int64_t timestamp_us_;
    size_t length_;
    int32_t pes_offset_;
    uint32_t pes_scrambling_control_;
  };

  Mode mode_;
  uint32_t flags_;
  bool eos_reached_;

  std::shared_ptr<Buffer> buffer_;
  std::list<RangeInfo> range_infos_;

  int32_t ca_system_id_;
  std::vector<uint8_t> cas_session_id_;

  std::shared_ptr<MediaMeta> format_;

  int au_index_;

  bool IsSampleEncrypted() const {
    return (flags_ & kFlag_SampleEncryptedData) != 0;
  }

  std::shared_ptr<Buffer> DequeueAccessUnitH264();
  std::shared_ptr<Buffer> DequeueAccessUnitHEVC();
  std::shared_ptr<Buffer> DequeueAccessUnitAAC();
  std::shared_ptr<Buffer> DequeueAccessUnitEAC3();
  std::shared_ptr<Buffer> DequeueAccessUnitAC3();
  std::shared_ptr<Buffer> DequeueAccessUnitAC4();
  std::shared_ptr<Buffer> DequeueAccessUnitMPEGAudio();
  std::shared_ptr<Buffer> DequeueAccessUnitMPEGVideo();
  std::shared_ptr<Buffer> DequeueAccessUnitMPEG4Video();
  std::shared_ptr<Buffer> DequeueAccessUnitPCMAudio();
  std::shared_ptr<Buffer> DequeueAccessUnitMetadata();

  // consume a logical (compressed) access unit of size "size",
  // returns its timestamp in us (or -1 if no time information).
  int64_t FetchTimestamp(size_t size,
                         int32_t* pes_offset = nullptr,
                         int32_t* pes_scrambling_control = nullptr);

  AVE_DISALLOW_COPY_AND_ASSIGN(ESQueue);
};

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave

#endif  // MODULES_MPEG2TS_ES_QUEUE_H
