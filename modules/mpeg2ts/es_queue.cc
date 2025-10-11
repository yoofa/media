/*
 * es_queue.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 *
 * Ported from Android MPEG2-TS module (ElementaryStreamQueue)
 * Original Copyright (C) 2010 The Android Open Source Project
 */

#include "es_queue.h"

#include <cstring>

#include "base/logging.h"
#include "foundation/avc_utils.h"
#include "foundation/bit_reader.h"
#include "foundation/buffer.h"
#include "foundation/media_defs.h"
#include "foundation/media_meta.h"

namespace ave {
namespace media {
namespace mpeg2ts {

ESQueue::ESQueue(Mode mode, uint32_t flags)
    : mode_(mode),
      flags_(flags),
      eos_reached_(false),
      ca_system_id_(0),
      au_index_(0) {
  LOG(INFO) << "ESQueue mode=" << static_cast<int>(mode) << " flags=" << flags
            << " is_scrambled=" << IsScrambled()
            << " is_sample_encrypted=" << IsSampleEncrypted();
}

ESQueue::~ESQueue() {}

std::shared_ptr<MediaMeta> ESQueue::GetFormat() {
  return format_;
}

void ESQueue::Clear(bool clear_format) {
  if (buffer_ != nullptr) {
    buffer_->setRange(0, 0);
  }

  range_infos_.clear();

  if (clear_format) {
    format_ = nullptr;
  }

  eos_reached_ = false;
}

bool ESQueue::IsScrambled() const {
  return (flags_ & kFlag_ScrambledData) != 0;
}

void ESQueue::SetCasInfo(int32_t system_id,
                         const std::vector<uint8_t>& session_id) {
  ca_system_id_ = system_id;
  cas_session_id_ = session_id;
}

status_t ESQueue::AppendData(const void* data,
                              size_t size,
                              int64_t time_us,
                              int32_t payload_offset,
                              uint32_t pes_scrambling_control) {
  if (buffer_ == nullptr || buffer_->size() + size > buffer_->capacity()) {
    size_t new_capacity = (buffer_ != nullptr) ? buffer_->capacity() : 0;

    while (new_capacity < size || buffer_->size() + size > new_capacity) {
      if (new_capacity < 8192) {
        new_capacity = 8192;
      } else if (new_capacity < 2 * 1024 * 1024) {
        new_capacity *= 2;
      } else {
        new_capacity += 1024 * 1024;
      }
    }

    auto new_buffer = std::make_shared<Buffer>(new_capacity);
    if (buffer_ != nullptr) {
      memcpy(new_buffer->data(), buffer_->data(), buffer_->size());
      new_buffer->setRange(0, buffer_->size());
    }
    buffer_ = new_buffer;
  }

  memcpy(buffer_->data() + buffer_->size(), data, size);
  buffer_->setRange(0, buffer_->size() + size);

  RangeInfo info;
  info.timestamp_us_ = time_us;
  info.length_ = size;
  info.pes_offset_ = payload_offset;
  info.pes_scrambling_control_ = pes_scrambling_control;
  range_infos_.push_back(info);

  return OK;
}

void ESQueue::SignalEOS() {
  eos_reached_ = true;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnit() {
  if ((flags_ & kFlag_AlignedData) != 0) {
    if (range_infos_.empty()) {
      return nullptr;
    }

    RangeInfo info = range_infos_.front();
    range_infos_.pop_front();

    auto access_unit = std::make_shared<Buffer>(info.length_);
    memcpy(access_unit->data(), buffer_->data(), info.length_);
    access_unit->setRange(0, info.length_);

    // Remove consumed data from buffer
    memmove(buffer_->data(), buffer_->data() + info.length_,
            buffer_->size() - info.length_);
    buffer_->setRange(0, buffer_->size() - info.length_);

    if (info.timestamp_us_ >= 0) {
      access_unit->meta()->setInt64("timeUs", info.timestamp_us_);
    }

    return access_unit;
  }

  switch (mode_) {
    case Mode::H264:
      return DequeueAccessUnitH264();
    case Mode::HEVC:
      return DequeueAccessUnitHEVC();
    case Mode::AAC:
      return DequeueAccessUnitAAC();
    case Mode::AC3:
      return DequeueAccessUnitAC3();
    case Mode::EAC3:
      return DequeueAccessUnitEAC3();
    case Mode::AC4:
      return DequeueAccessUnitAC4();
    case Mode::MPEG_AUDIO:
      return DequeueAccessUnitMPEGAudio();
    case Mode::MPEG_VIDEO:
      return DequeueAccessUnitMPEGVideo();
    case Mode::MPEG4_VIDEO:
      return DequeueAccessUnitMPEG4Video();
    case Mode::PCM_AUDIO:
      return DequeueAccessUnitPCMAudio();
    case Mode::METADATA:
      return DequeueAccessUnitMetadata();
    default:
      LOG(ERROR) << "Unknown mode: " << static_cast<int>(mode_);
      return nullptr;
  }
}

int64_t ESQueue::FetchTimestamp(size_t size,
                                int32_t* pes_offset,
                                int32_t* pes_scrambling_control) {
  int64_t time_us = -1;
  bool first = true;

  while (size > 0) {
    if (range_infos_.empty()) {
      return time_us;
    }

    RangeInfo& info = range_infos_.front();

    if (first) {
      time_us = info.timestamp_us_;
      if (pes_offset) {
        *pes_offset = info.pes_offset_;
      }
      if (pes_scrambling_control) {
        *pes_scrambling_control = info.pes_scrambling_control_;
      }
      first = false;
    }

    if (info.length_ > size) {
      info.length_ -= size;
      size = 0;
    } else {
      size -= info.length_;
      range_infos_.pop_front();
    }
  }

  return time_us;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitH264() {
  if (buffer_ == nullptr || buffer_->size() == 0) {
    return nullptr;
  }

  // Simple implementation: look for NAL unit start codes
  const uint8_t* data = buffer_->data();
  size_t size = buffer_->size();

  // Look for start code: 0x00 0x00 0x00 0x01 or 0x00 0x00 0x01
  size_t nal_start = 0;
  bool found_start = false;

  for (size_t i = 0; i + 3 < size; ++i) {
    if (data[i] == 0 && data[i + 1] == 0) {
      if (data[i + 2] == 1) {
        nal_start = i + 3;
        found_start = true;
        break;
      } else if (i + 4 < size && data[i + 2] == 0 && data[i + 3] == 1) {
        nal_start = i + 4;
        found_start = true;
        break;
      }
    }
  }

  if (!found_start) {
    if (eos_reached_) {
      // Return whatever we have
      auto access_unit = std::make_shared<Buffer>(size);
      memcpy(access_unit->data(), data, size);
      access_unit->setRange(0, size);
      int64_t time_us = FetchTimestamp(size);
      if (time_us >= 0) {
        access_unit->meta()->setInt64("timeUs", time_us);
      }
      buffer_->setRange(0, 0);
      return access_unit;
    }
    return nullptr;
  }

  // Find next start code (end of this NAL unit)
  size_t nal_end = size;
  for (size_t i = nal_start; i + 3 < size; ++i) {
    if (data[i] == 0 && data[i + 1] == 0) {
      if (data[i + 2] == 1) {
        nal_end = i;
        break;
      } else if (i + 4 < size && data[i + 2] == 0 && data[i + 3] == 1) {
        nal_end = i;
        break;
      }
    }
  }

  // Need more data if we haven't found the end and haven't reached EOS
  if (nal_end == size && !eos_reached_) {
    return nullptr;
  }

  // Extract the access unit
  size_t au_size = nal_end;
  auto access_unit = std::make_shared<Buffer>(au_size);
  memcpy(access_unit->data(), data, au_size);
  access_unit->setRange(0, au_size);

  int64_t time_us = FetchTimestamp(au_size);
  if (time_us >= 0) {
    access_unit->meta()->setInt64("timeUs", time_us);
  }

  // Remove consumed data
  memmove(buffer_->data(), buffer_->data() + au_size, size - au_size);
  buffer_->setRange(0, size - au_size);

  // Initialize format if not set
  if (!format_) {
    format_ = MediaMeta::CreatePtr(MediaType::VIDEO, MediaMeta::FormatType::kTrack);
    format_->SetMime(MEDIA_MIMETYPE_VIDEO_AVC);
  }

  return access_unit;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitHEVC() {
  // Similar to H264, but with HEVC NAL units
  // Simplified implementation
  return DequeueAccessUnitH264();  // Use H264 logic as placeholder
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitAAC() {
  if (buffer_ == nullptr || buffer_->size() < 7) {
    return nullptr;
  }

  const uint8_t* data = buffer_->data();
  size_t size = buffer_->size();

  // Look for ADTS sync word: 0xFFF
  size_t offset = 0;
  while (offset + 7 <= size) {
    if ((data[offset] == 0xFF) && ((data[offset + 1] & 0xF6) == 0xF0)) {
      // Found ADTS header
      size_t frame_length =
          ((data[offset + 3] & 0x03) << 11) | (data[offset + 4] << 3) |
          ((data[offset + 5] & 0xE0) >> 5);

      if (offset + frame_length > size) {
        if (eos_reached_) {
          return nullptr;
        }
        // Need more data
        return nullptr;
      }

      // Extract the frame
      auto access_unit = std::make_shared<Buffer>(frame_length);
      memcpy(access_unit->data(), data + offset, frame_length);
      access_unit->setRange(0, frame_length);

      int64_t time_us = FetchTimestamp(frame_length + offset);
      if (time_us >= 0) {
        access_unit->meta()->setInt64("timeUs", time_us);
      }

      // Remove consumed data
      memmove(buffer_->data(), buffer_->data() + offset + frame_length,
              size - offset - frame_length);
      buffer_->setRange(0, size - offset - frame_length);

      // Initialize format if not set
      if (!format_) {
        format_ = MediaMeta::CreatePtr(MediaType::AUDIO, MediaMeta::FormatType::kTrack);
        format_->SetMime(MEDIA_MIMETYPE_AUDIO_AAC);
        // Parse sample rate and channels from ADTS header
        uint8_t sampling_frequency_index = (data[offset + 2] & 0x3C) >> 2;
        uint8_t channel_configuration =
            ((data[offset + 2] & 0x01) << 2) | ((data[offset + 3] & 0xC0) >> 6);

        static const uint32_t kSamplingFrequencyTable[] = {
            96000, 88200, 64000, 48000, 44100, 32000, 24000,
            22050, 16000, 12000, 11025, 8000,  7350,  0,
        };

        if (sampling_frequency_index < 13) {
          format_->SetSampleRate(
              kSamplingFrequencyTable[sampling_frequency_index]);
        }

        if (channel_configuration > 0 && channel_configuration <= 7) {
          format_->SetChannelLayout(
              static_cast<ChannelLayout>(channel_configuration));
        }
      }

      return access_unit;
    }
    ++offset;
  }

  if (eos_reached_ && size > 0) {
    // Return whatever we have
    auto access_unit = std::make_shared<Buffer>(size);
    memcpy(access_unit->data(), data, size);
    access_unit->setRange(0, size);
    int64_t time_us = FetchTimestamp(size);
    if (time_us >= 0) {
      access_unit->meta()->setInt64("timeUs", time_us);
    }
    buffer_->setRange(0, 0);
    return access_unit;
  }

  return nullptr;
}

// Placeholder implementations for other formats
std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitAC3() {
  LOG(WARNING) << "AC3 dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitEAC3() {
  LOG(WARNING) << "EAC3 dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitAC4() {
  LOG(WARNING) << "AC4 dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitMPEGAudio() {
  LOG(WARNING) << "MPEG Audio dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitMPEGVideo() {
  LOG(WARNING) << "MPEG Video dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitMPEG4Video() {
  LOG(WARNING) << "MPEG4 Video dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitPCMAudio() {
  LOG(WARNING) << "PCM Audio dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<Buffer> ESQueue::DequeueAccessUnitMetadata() {
  LOG(WARNING) << "Metadata dequeue not fully implemented";
  return nullptr;
}

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave
