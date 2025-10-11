/*
 * ts_parser.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 *
 * Ported from Android MPEG2-TS module (ATSParser)
 * Original Copyright (C) 2010 The Android Open Source Project
 */

#include "ts_parser.h"

#include <cstring>

#include "base/logging.h"
#include "es_queue.h"
#include "foundation/bit_reader.h"
#include "foundation/buffer.h"
#include "foundation/media_defs.h"
#include "foundation/media_meta.h"
#include "foundation/message.h"
#include "packet_source.h"

namespace ave {
namespace media {
namespace mpeg2ts {

static const size_t kTSPacketSize = 188;

// Internal classes
class TSParser::PSISection {
 public:
  PSISection() : skip_bytes_(0) {}

  status_t Append(const void* data, size_t size) {
    if (!buffer_) {
      buffer_ = std::make_shared<Buffer>(size);
      memcpy(buffer_->data(), data, size);
      buffer_->setRange(0, size);
    } else {
      size_t new_size = buffer_->size() + size;
      if (new_size > buffer_->capacity()) {
        auto new_buffer = std::make_shared<Buffer>(new_size);
        memcpy(new_buffer->data(), buffer_->data(), buffer_->size());
        buffer_ = new_buffer;
      }
      memcpy(buffer_->data() + buffer_->size(), data, size);
      buffer_->setRange(0, new_size);
    }
    return OK;
  }

  void SetSkipBytes(uint8_t skip) { skip_bytes_ = skip; }

  void Clear() {
    if (buffer_) {
      buffer_->setRange(0, 0);
    }
  }

  bool IsComplete() const {
    if (!buffer_ || buffer_->size() < 3) {
      return false;
    }

    unsigned section_length =
        (((buffer_->data()[1] & 0x0f) << 8) | buffer_->data()[2]);
    return buffer_->size() >= section_length + 3;
  }

  bool IsEmpty() const { return !buffer_ || buffer_->size() == 0; }

  const uint8_t* Data() const { return buffer_ ? buffer_->data() : nullptr; }

  size_t Size() const { return buffer_ ? buffer_->size() : 0; }

 private:
  std::shared_ptr<Buffer> buffer_;
  uint8_t skip_bytes_;
};

class TSParser::Stream {
 public:
  Stream(Program* program, unsigned pid, unsigned stream_type)
      : program_(program),
        elementary_pid_(pid),
        stream_type_(stream_type),
        expected_continuity_counter_(-1),
        payload_started_(false),
        eos_reached_(false),
        prev_pts_(0) {
    // Create appropriate ES queue based on stream type
    ESQueue::Mode mode = ESQueue::Mode::INVALID;

    switch (stream_type) {
      case STREAMTYPE_H264:
        mode = ESQueue::Mode::H264;
        break;
      case STREAMTYPE_H265:
        mode = ESQueue::Mode::HEVC;
        break;
      case STREAMTYPE_MPEG2_AUDIO_ADTS:
        mode = ESQueue::Mode::AAC;
        break;
      case STREAMTYPE_AC3:
        mode = ESQueue::Mode::AC3;
        break;
      case STREAMTYPE_EAC3:
        mode = ESQueue::Mode::EAC3;
        break;
      case STREAMTYPE_MPEG1_AUDIO:
      case STREAMTYPE_MPEG2_AUDIO:
        mode = ESQueue::Mode::MPEG_AUDIO;
        break;
      case STREAMTYPE_MPEG1_VIDEO:
      case STREAMTYPE_MPEG2_VIDEO:
        mode = ESQueue::Mode::MPEG_VIDEO;
        break;
      case STREAMTYPE_MPEG4_VIDEO:
        mode = ESQueue::Mode::MPEG4_VIDEO;
        break;
      default:
        LOG(WARNING) << "Unsupported stream type: " << stream_type;
        break;
    }

    if (mode != ESQueue::Mode::INVALID) {
      queue_ = std::make_unique<ESQueue>(mode);
    }

    // Create source
    auto meta = MediaMeta::CreatePtr(
        IsVideo() ? MediaType::VIDEO : MediaType::AUDIO,
        MediaMeta::FormatType::kTrack);
    source_ = std::make_shared<PacketSource>(meta);
  }

  unsigned Type() const { return stream_type_; }
  unsigned Pid() const { return elementary_pid_; }

  std::shared_ptr<PacketSource> GetSource() { return source_; }

  bool IsVideo() const {
    return stream_type_ == STREAMTYPE_H264 ||
           stream_type_ == STREAMTYPE_H265 ||
           stream_type_ == STREAMTYPE_MPEG1_VIDEO ||
           stream_type_ == STREAMTYPE_MPEG2_VIDEO ||
           stream_type_ == STREAMTYPE_MPEG4_VIDEO;
  }

  bool IsAudio() const {
    return stream_type_ == STREAMTYPE_MPEG1_AUDIO ||
           stream_type_ == STREAMTYPE_MPEG2_AUDIO ||
           stream_type_ == STREAMTYPE_MPEG2_AUDIO_ADTS ||
           stream_type_ == STREAMTYPE_AC3 || stream_type_ == STREAMTYPE_EAC3;
  }

  status_t Parse(unsigned continuity_counter,
                 unsigned payload_unit_start_indicator,
                 BitReader* br,
                 TSParser::SyncEvent* event) {
    if (expected_continuity_counter_ >= 0 &&
        (unsigned)expected_continuity_counter_ != continuity_counter) {
      LOG(WARNING) << "Discontinuity on stream PID " << elementary_pid_;
      payload_started_ = false;
      buffer_ = nullptr;
      expected_continuity_counter_ = -1;
    }

    expected_continuity_counter_ = (continuity_counter + 1) & 0x0f;

    if (payload_unit_start_indicator) {
      if (buffer_ && buffer_->size() > 0) {
        Flush(event);
      }
      payload_started_ = true;
    }

    if (!payload_started_) {
      return OK;
    }

    size_t payload_size = br->numBitsLeft() / 8;
    if (payload_size == 0) {
      return OK;
    }

    status_t err = ParsePES(br, event);
    if (err != OK) {
      return err;
    }

    return OK;
  }

  void SignalDiscontinuity(DiscontinuityType type,
                           std::shared_ptr<Message> extra) {
    payload_started_ = false;
    buffer_ = nullptr;
    expected_continuity_counter_ = -1;

    if (source_) {
      source_->QueueDiscontinuity(type, extra, false);
    }
  }

  void SignalEOS(status_t final_result) {
    if (queue_) {
      queue_->SignalEOS();
      Flush(nullptr);
    }

    if (source_) {
      source_->SignalEOS(final_result);
    }

    eos_reached_ = true;
  }

 private:
  Program* program_;
  unsigned elementary_pid_;
  unsigned stream_type_;
  int32_t expected_continuity_counter_;

  std::shared_ptr<Buffer> buffer_;
  std::shared_ptr<PacketSource> source_;
  bool payload_started_;
  bool eos_reached_;

  uint64_t prev_pts_;
  std::unique_ptr<ESQueue> queue_;

  status_t ParsePES(BitReader* br, TSParser::SyncEvent* event) {
    if (br->numBitsLeft() < 3 * 8) {
      return ERROR_MALFORMED;
    }

    uint32_t packet_start_code_prefix = br->getBits(24);
    if (packet_start_code_prefix != 1) {
      LOG(VERBOSE) << "Not a valid PES start code";
      // Just buffer the data for now
      size_t payload_size = br->numBitsLeft() / 8;
      const uint8_t* data_ptr = br->data() + (br->numBitsLeft() / 8);

      if (queue_) {
        queue_->AppendData(data_ptr, payload_size, -1);
      }
      return OK;
    }

    if (br->numBitsLeft() < (8 + 16 + 8 + 8) * 8) {
      return ERROR_MALFORMED;
    }

    br->skipBits(8);  // stream_id
    unsigned pes_packet_length = br->getBits(16);
    (void)pes_packet_length;

    if (br->numBitsLeft() < 16) {
      return ERROR_MALFORMED;
    }

    uint32_t marker_bits = br->getBits(2);
    if (marker_bits != 2) {
      return ERROR_MALFORMED;
    }

    br->skipBits(2);   // PES_scrambling_control
    br->skipBits(1);   // PES_priority
    br->skipBits(1);   // data_alignment_indicator
    br->skipBits(1);   // copyright
    br->skipBits(1);   // original_or_copy

    unsigned pts_dts_flags = br->getBits(2);
    br->skipBits(6);  // other flags

    unsigned pes_header_data_length = br->getBits(8);

    uint64_t pts = 0;
    uint64_t dts = 0;

    if (pts_dts_flags == 2 || pts_dts_flags == 3) {
      if (br->numBitsLeft() < 40) {
        return ERROR_MALFORMED;
      }

      br->skipBits(4);  // '0010' or '0011'
      uint64_t pts_32_30 = br->getBits(3);
      br->skipBits(1);  // marker_bit
      uint64_t pts_29_15 = br->getBits(15);
      br->skipBits(1);  // marker_bit
      uint64_t pts_14_0 = br->getBits(15);
      br->skipBits(1);  // marker_bit

      pts = (pts_32_30 << 30) | (pts_29_15 << 15) | pts_14_0;

      if (pts_dts_flags == 3) {
        if (br->numBitsLeft() < 40) {
          return ERROR_MALFORMED;
        }
        // Parse DTS similarly
        br->skipBits(40);
      }
    }

    // Skip remaining PES header
    size_t remaining_header = pes_header_data_length;
    if (pts_dts_flags == 2) {
      remaining_header -= 5;
    } else if (pts_dts_flags == 3) {
      remaining_header -= 10;
    }

    if (br->numBitsLeft() < remaining_header * 8) {
      return ERROR_MALFORMED;
    }
    br->skipBits(remaining_header * 8);

    // Now we have the payload
    size_t payload_size = br->numBitsLeft() / 8;
    if (payload_size == 0) {
      return OK;
    }

    const uint8_t* data_ptr = br->data() + (br->numBitsLeft() / 8);

    if (queue_) {
      int64_t time_us = -1;
      if (pts != 0) {
        // Convert 90kHz PTS to microseconds
        time_us = (pts * 1000000LL) / 90000;
      }

      queue_->AppendData(data_ptr, payload_size, time_us);

      auto access_unit = queue_->DequeueAccessUnit();
      while (access_unit) {
        source_->QueueAccessUnit(access_unit);

        // Check if this is a sync frame
        int64_t au_time_us;
        if (access_unit->meta()->findInt64("timeUs", &au_time_us) && event &&
            !event->HasReturnedData()) {
          // This is a simplified check - in reality we'd need to parse the NAL
          // units
          event->Init(0, source_, au_time_us,
                      IsVideo() ? SourceType::VIDEO : SourceType::AUDIO);
        }

        access_unit = queue_->DequeueAccessUnit();
      }
    }

    return OK;
  }

  status_t Flush(TSParser::SyncEvent* event) {
    if (queue_) {
      auto access_unit = queue_->DequeueAccessUnit();
      while (access_unit) {
        source_->QueueAccessUnit(access_unit);
        access_unit = queue_->DequeueAccessUnit();
      }
    }

    buffer_ = nullptr;
    return OK;
  }
};

class TSParser::Program {
 public:
  Program(TSParser* parser, unsigned program_number, unsigned program_map_pid)
      : parser_(parser),
        program_number_(program_number),
        program_map_pid_(program_map_pid),
        first_pts_valid_(false),
        first_pts_(0) {}

  bool ParsePSISection(unsigned pid, BitReader* br, status_t* err) {
    *err = OK;

    if (pid == program_map_pid_) {
      *err = ParseProgramMap(br);
      return true;
    }

    return false;
  }

  bool ParsePID(unsigned pid,
                unsigned continuity_counter,
                unsigned payload_unit_start_indicator,
                unsigned transport_scrambling_control,
                unsigned random_access_indicator,
                BitReader* br,
                status_t* err,
                TSParser::SyncEvent* event) {
    *err = OK;

    auto it = streams_.find(pid);
    if (it == streams_.end()) {
      return false;
    }

    *err = it->second->Parse(continuity_counter, payload_unit_start_indicator,
                             br, event);
    return true;
  }

  void SignalDiscontinuity(DiscontinuityType type,
                           std::shared_ptr<Message> extra) {
    for (auto& pair : streams_) {
      pair.second->SignalDiscontinuity(type, extra);
    }
  }

  void SignalEOS(status_t final_result) {
    for (auto& pair : streams_) {
      pair.second->SignalEOS(final_result);
    }
  }

  std::shared_ptr<PacketSource> GetSource(TSParser::SourceType type) {
    for (auto& pair : streams_) {
      if (type == TSParser::VIDEO && pair.second->IsVideo()) {
        return pair.second->GetSource();
      } else if (type == TSParser::AUDIO && pair.second->IsAudio()) {
        return pair.second->GetSource();
      }
    }
    return nullptr;
  }

  bool HasSource(TSParser::SourceType type) const {
    for (const auto& pair : streams_) {
      if (type == TSParser::VIDEO && pair.second->IsVideo()) {
        return true;
      } else if (type == TSParser::AUDIO && pair.second->IsAudio()) {
        return true;
      }
    }
    return false;
  }

 private:
  TSParser* parser_;
  unsigned program_number_;
  unsigned program_map_pid_;
  std::map<unsigned, std::shared_ptr<Stream>> streams_;
  bool first_pts_valid_;
  uint64_t first_pts_;

  status_t ParseProgramMap(BitReader* br) {
    // Simplified PMT parsing
    if (br->numBitsLeft() < 8 * 8) {
      return ERROR_MALFORMED;
    }

    br->skipBits(8);  // table_id
    br->skipBits(4);  // various flags
    unsigned section_length = br->getBits(12);

    if (br->numBitsLeft() < section_length * 8) {
      return ERROR_MALFORMED;
    }

    br->skipBits(16);  // program_number
    br->skipBits(2);   // reserved
    br->skipBits(5);   // version_number
    br->skipBits(1);   // current_next_indicator
    br->skipBits(8);   // section_number
    br->skipBits(8);   // last_section_number

    br->skipBits(3);  // reserved
    br->skipBits(13);  // PCR_PID

    br->skipBits(4);  // reserved
    unsigned program_info_length = br->getBits(12);
    br->skipBits(program_info_length * 8);  // skip program info

    // Parse stream info
    size_t info_bytes_remaining = section_length - 9 - program_info_length - 4;

    while (info_bytes_remaining >= 5) {
      if (br->numBitsLeft() < 40) {
        return ERROR_MALFORMED;
      }

      unsigned stream_type = br->getBits(8);
      br->skipBits(3);  // reserved
      unsigned elementary_pid = br->getBits(13);
      br->skipBits(4);  // reserved
      unsigned es_info_length = br->getBits(12);

      if (br->numBitsLeft() < es_info_length * 8) {
        return ERROR_MALFORMED;
      }
      br->skipBits(es_info_length * 8);

      // Create stream if not exists
      if (streams_.find(elementary_pid) == streams_.end()) {
        auto stream =
            std::make_shared<Stream>(this, elementary_pid, stream_type);
        streams_[elementary_pid] = stream;
        LOG(INFO) << "Found stream: PID=" << elementary_pid
                  << " type=" << stream_type;
      }

      info_bytes_remaining -= 5 + es_info_length;
    }

    return OK;
  }
};

// TSParser implementation
TSParser::SyncEvent::SyncEvent(int64_t offset)
    : has_returned_data_(false), offset_(offset), time_us_(0) {}

void TSParser::SyncEvent::Init(int64_t offset,
                               std::shared_ptr<PacketSource> source,
                               int64_t time_us,
                               SourceType type) {
  has_returned_data_ = true;
  offset_ = offset;
  media_source_ = source;
  time_us_ = time_us;
  type_ = type;
}

void TSParser::SyncEvent::Reset() {
  has_returned_data_ = false;
}

TSParser::TSParser(uint32_t flags)
    : flags_(flags),
      absolute_time_anchor_us_(0),
      time_offset_valid_(false),
      time_offset_us_(0),
      last_recovered_pts_(0),
      num_ts_packets_parsed_(0),
      num_pcrs_(0) {
  pcr_[0] = pcr_[1] = 0;
  pcr_bytes_[0] = pcr_bytes_[1] = 0;
  system_time_us_[0] = system_time_us_[1] = 0;
}

TSParser::~TSParser() {}

status_t TSParser::FeedTSPacket(const void* data,
                                size_t size,
                                SyncEvent* event) {
  if (size != kTSPacketSize) {
    LOG(ERROR) << "Invalid TS packet size: " << size;
    return ERROR_MALFORMED;
  }

  BitReader br(static_cast<const uint8_t*>(data), kTSPacketSize);
  return ParseTS(&br, event);
}

status_t TSParser::ParseTS(BitReader* br, SyncEvent* event) {
  if (br->getBits(8) != 0x47) {
    LOG(ERROR) << "TS sync byte not found";
    return ERROR_MALFORMED;
  }

  if (br->numBitsLeft() < 3 + 1 + 1 + 13 + 2 + 2 + 4) {
    return ERROR_MALFORMED;
  }

  br->skipBits(1);  // transport_error_indicator
  unsigned payload_unit_start_indicator = br->getBits(1);
  br->skipBits(1);  // transport_priority

  unsigned pid = br->getBits(13);

  unsigned transport_scrambling_control = br->getBits(2);
  unsigned adaptation_field_control = br->getBits(2);
  unsigned continuity_counter = br->getBits(4);

  unsigned random_access_indicator = 0;
  if (adaptation_field_control == 2 || adaptation_field_control == 3) {
    status_t err = ParseAdaptationField(br, pid, &random_access_indicator);
    if (err != OK) {
      return err;
    }
  }

  status_t err = OK;

  if (adaptation_field_control == 1 || adaptation_field_control == 3) {
    err = ParsePID(br, pid, continuity_counter, payload_unit_start_indicator,
                   transport_scrambling_control, random_access_indicator,
                   event);
  }

  ++num_ts_packets_parsed_;

  return err;
}

status_t TSParser::ParsePID(BitReader* br,
                            unsigned pid,
                            unsigned continuity_counter,
                            unsigned payload_unit_start_indicator,
                            unsigned transport_scrambling_control,
                            unsigned random_access_indicator,
                            SyncEvent* event) {
  if (pid == 0) {
    // PAT
    if (payload_unit_start_indicator) {
      unsigned skip = br->getBits(8);
      br->skipBits(skip * 8);
    }
    ParseProgramAssociationTable(br);
    return OK;
  }

  // Check if this is a PMT or stream PID
  status_t err;
  for (auto& program : programs_) {
    if (program->ParsePSISection(pid, br, &err)) {
      return err;
    }

    if (program->ParsePID(pid, continuity_counter,
                          payload_unit_start_indicator,
                          transport_scrambling_control, random_access_indicator,
                          br, &err, event)) {
      return err;
    }
  }

  return OK;
}

void TSParser::ParseProgramAssociationTable(BitReader* br) {
  if (br->numBitsLeft() < 8 * 8) {
    return;
  }

  br->skipBits(8);  // table_id
  br->skipBits(4);  // section_syntax_indicator, '0', reserved
  unsigned section_length = br->getBits(12);

  if (br->numBitsLeft() < section_length * 8) {
    return;
  }

  br->skipBits(16);  // transport_stream_id
  br->skipBits(2);   // reserved
  br->skipBits(5);   // version_number
  br->skipBits(1);   // current_next_indicator
  br->skipBits(8);   // section_number
  br->skipBits(8);   // last_section_number

  size_t num_program_bytes = section_length - 5 - 4;  // 4 for CRC

  while (num_program_bytes >= 4) {
    unsigned program_number = br->getBits(16);
    br->skipBits(3);  // reserved
    unsigned program_map_pid = br->getBits(13);

    if (program_number != 0) {
      // Check if we already have this program
      bool found = false;
      for (auto& program : programs_) {
        if (program->HasSource(VIDEO) || program->HasSource(AUDIO)) {
          found = true;
          break;
        }
      }

      if (!found) {
        auto program = std::make_shared<Program>(this, program_number,
                                                  program_map_pid);
        programs_.push_back(program);
        LOG(INFO) << "Found program " << program_number << " with PMT PID "
                  << program_map_pid;
      }
    }

    num_program_bytes -= 4;
  }
}

status_t TSParser::ParseAdaptationField(BitReader* br,
                                        unsigned pid,
                                        unsigned* random_access_indicator) {
  *random_access_indicator = 0;

  if (br->numBitsLeft() < 8) {
    return ERROR_MALFORMED;
  }

  unsigned adaptation_field_length = br->getBits(8);

  if (adaptation_field_length > 0) {
    if (br->numBitsLeft() < 8) {
      return ERROR_MALFORMED;
    }

    br->skipBits(1);  // discontinuity_indicator
    *random_access_indicator = br->getBits(1);
    br->skipBits(6);  // other flags

    // Skip rest of adaptation field
    size_t remaining = adaptation_field_length - 1;
    if (br->numBitsLeft() < remaining * 8) {
      return ERROR_MALFORMED;
    }
    br->skipBits(remaining * 8);
  }

  return OK;
}

void TSParser::SignalDiscontinuity(DiscontinuityType type,
                                   std::shared_ptr<Message> extra) {
  for (auto& program : programs_) {
    program->SignalDiscontinuity(type, extra);
  }
}

void TSParser::SignalEOS(status_t final_result) {
  for (auto& program : programs_) {
    program->SignalEOS(final_result);
  }
}

std::shared_ptr<PacketSource> TSParser::GetSource(SourceType type) {
  for (auto& program : programs_) {
    auto source = program->GetSource(type);
    if (source) {
      return source;
    }
  }
  return nullptr;
}

bool TSParser::HasSource(SourceType type) const {
  for (const auto& program : programs_) {
    if (program->HasSource(type)) {
      return true;
    }
  }
  return false;
}

bool TSParser::PTSTimeDeltaEstablished() {
  // Simplified: just return true if we have at least parsed some packets
  return num_ts_packets_parsed_ > 10;
}

int64_t TSParser::GetFirstPTSTimeUs() {
  // Simplified implementation
  return 0;
}

void TSParser::UpdatePCR(unsigned pid,
                         uint64_t pcr,
                         uint64_t byte_offset_from_start) {
  // PCR handling - simplified
}

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave
