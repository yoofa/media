/*
 * ts_parser.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 *
 * Ported from Android MPEG2-TS module (ATSParser)
 * Original Copyright (C) 2010 The Android Open Source Project
 */

#ifndef MODULES_MPEG2TS_TS_PARSER_H
#define MODULES_MPEG2TS_TS_PARSER_H

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "base/constructor_magic.h"
#include "foundation/media_errors.h"
#include "packet_source.h"

namespace ave {
namespace media {

class Message;

namespace mpeg2ts {

class ESQueue;
class PacketSource;

class TSParser {
 public:
  enum Flags {
    // The 90kHz clock (PTS/DTS) is absolute, i.e. PTS=0 corresponds to
    // a media time of 0.
    // If this flag is _not_ specified, the first PTS encountered in a
    // program of this stream will be assumed to correspond to media time 0
    // instead.
    TS_TIMESTAMPS_ARE_ABSOLUTE = 1,
    // Video PES packets contain exactly one (aligned) access unit.
    ALIGNED_VIDEO_DATA = 2,
  };

  enum SourceType {
    VIDEO = 0,
    AUDIO = 1,
    META = 2,
    NUM_SOURCE_TYPES = 3
  };

  // Event is used to signal sync point event at FeedTSPacket().
  struct SyncEvent {
    explicit SyncEvent(int64_t offset);

    void Init(int64_t offset,
              std::shared_ptr<PacketSource> source,
              int64_t time_us,
              SourceType type);

    bool HasReturnedData() const { return has_returned_data_; }
    void Reset();
    int64_t GetOffset() const { return offset_; }
    std::shared_ptr<PacketSource> GetMediaSource() const {
      return media_source_;
    }
    int64_t GetTimeUs() const { return time_us_; }
    SourceType GetType() const { return type_; }

   private:
    bool has_returned_data_;
    int64_t offset_;
    std::shared_ptr<PacketSource> media_source_;
    int64_t time_us_;
    SourceType type_;
  };

  explicit TSParser(uint32_t flags = 0);
  virtual ~TSParser();

  // Feed a TS packet into the parser. uninitialized event with the start
  // offset of this TS packet goes in, and if the parser detects PES with
  // a sync frame, the event will be initiailzed with the start offset of the
  // PES. Note that the offset of the event can be different from what we fed,
  // as a PES may consist of multiple TS packets.
  status_t FeedTSPacket(const void* data,
                        size_t size,
                        SyncEvent* event = nullptr);

  void SignalDiscontinuity(DiscontinuityType type,
                           std::shared_ptr<Message> extra);

  void SignalEOS(status_t final_result);

  std::shared_ptr<PacketSource> GetSource(SourceType type);
  bool HasSource(SourceType type) const;

  bool PTSTimeDeltaEstablished();

  int64_t GetFirstPTSTimeUs();

  // MPEG2-TS Stream types (from ISO/IEC 13818-1: 2000 (E), Table 2-29)
  enum {
    STREAMTYPE_RESERVED = 0x00,
    STREAMTYPE_MPEG1_VIDEO = 0x01,
    STREAMTYPE_MPEG2_VIDEO = 0x02,
    STREAMTYPE_MPEG1_AUDIO = 0x03,
    STREAMTYPE_MPEG2_AUDIO = 0x04,
    STREAMTYPE_PES_PRIVATE_DATA = 0x06,
    STREAMTYPE_MPEG2_AUDIO_ADTS = 0x0f,
    STREAMTYPE_MPEG4_VIDEO = 0x10,
    STREAMTYPE_METADATA = 0x15,
    STREAMTYPE_H264 = 0x1b,
    STREAMTYPE_H265 = 0x24,

    // From ATSC A/53 Part 3:2009, 6.7.1
    STREAMTYPE_AC3 = 0x81,
    STREAMTYPE_EAC3 = 0x87,
  };

 private:
  class Program;
  class Stream;
  class PSISection;

  struct StreamInfo {
    unsigned type;
    unsigned type_ext;
    unsigned pid;
  };

  uint32_t flags_;
  std::vector<std::shared_ptr<Program>> programs_;

  // Keyed by PID
  std::map<unsigned, std::shared_ptr<PSISection>> psi_sections_;

  int64_t absolute_time_anchor_us_;

  bool time_offset_valid_;
  int64_t time_offset_us_;
  int64_t last_recovered_pts_;

  size_t num_ts_packets_parsed_;

  void ParseProgramAssociationTable(base::BitReader* br);
  void ParseProgramMap(base::BitReader* br);
  void ParsePES(base::BitReader* br, SyncEvent* event);

  status_t ParsePID(base::BitReader* br,
                    unsigned pid,
                    unsigned continuity_counter,
                    unsigned payload_unit_start_indicator,
                    unsigned transport_scrambling_control,
                    unsigned random_access_indicator,
                    SyncEvent* event);

  status_t ParseAdaptationField(base::BitReader* br,
                                unsigned pid,
                                unsigned* random_access_indicator);

  status_t ParseTS(base::BitReader* br, SyncEvent* event);

  void UpdatePCR(unsigned pid, uint64_t pcr, uint64_t byte_offset_from_start);

  uint64_t pcr_[2];
  uint64_t pcr_bytes_[2];
  int64_t system_time_us_[2];
  size_t num_pcrs_;

  AVE_DISALLOW_COPY_AND_ASSIGN(TSParser);
};

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave

#endif  // MODULES_MPEG2TS_TS_PARSER_H
