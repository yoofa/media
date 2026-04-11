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
#include <vector>

#include "audio/channel_layout.h"
#include "base/logging.h"
#include "foundation/aac_utils.h"
#include "foundation/avc_utils.h"
#include "foundation/bit_reader.h"
#include "foundation/media_defs.h"
#include "foundation/media_frame.h"
#include "foundation/media_meta.h"

namespace ave {
namespace media {
namespace mpeg2ts {

namespace {

constexpr uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

struct H264NalUnit {
  const uint8_t* data = nullptr;
  size_t size = 0;
  unsigned type = 0;
};

struct LatmStreamMuxConfig {
  std::vector<uint8_t> audio_specific_config;
  uint32_t sample_rate = 0;
  uint8_t channel_count = 0;
  uint32_t num_subframes = 0;
  uint32_t frame_length_type = 0;
  bool other_data_present = false;
  uint64_t other_data_len_bits = 0;
};

std::shared_ptr<MediaFrame> CreateFrameCopy(const uint8_t* data,
                                            size_t size,
                                            MediaType media_type) {
  return MediaFrame::CreateSharedAsCopy(const_cast<uint8_t*>(data), size,
                                        media_type);
}

bool ParseUnsignedExpGolomb(NALBitReader* br, uint32_t* value) {
  uint32_t num_zeroes = 0;
  uint32_t bit = 0;
  while (true) {
    if (!br->getBitsGraceful(1, &bit)) {
      return false;
    }
    if (bit != 0) {
      break;
    }
    ++num_zeroes;
    if (num_zeroes >= 32) {
      return false;
    }
  }

  uint32_t suffix = 0;
  if (num_zeroes > 0 && !br->getBitsGraceful(num_zeroes, &suffix)) {
    return false;
  }

  *value = ((1u << num_zeroes) - 1u) + suffix;
  return true;
}

bool IsH264VclNal(unsigned nal_type) {
  return nal_type == 1 || nal_type == 5;
}

bool IsFirstH264SliceInAccessUnit(const uint8_t* nal_data, size_t nal_size) {
  if (nal_size <= 1) {
    return false;
  }

  NALBitReader br(nal_data + 1, nal_size - 1);
  uint32_t first_mb_in_slice = 0;
  return ParseUnsignedExpGolomb(&br, &first_mb_in_slice) &&
         first_mb_in_slice == 0;
}

bool ParseH264SliceType(const uint8_t* nal_data,
                        size_t nal_size,
                        uint32_t* slice_type) {
  if (nal_size <= 1 || slice_type == nullptr) {
    return false;
  }

  NALBitReader br(nal_data + 1, nal_size - 1);
  uint32_t first_mb_in_slice = 0;
  if (!ParseUnsignedExpGolomb(&br, &first_mb_in_slice)) {
    return false;
  }

  return ParseUnsignedExpGolomb(&br, slice_type);
}

bool IsH264ISliceType(uint32_t slice_type) {
  const uint32_t normalized = slice_type % 5;
  return normalized == 2 || normalized == 4;
}

bool StartsNewH264AccessUnit(unsigned nal_type, bool have_vcl) {
  if (nal_type == 9) {
    return have_vcl;
  }

  if (have_vcl && (nal_type == 6 || nal_type == 7 || nal_type == 8)) {
    return true;
  }

  return false;
}

size_t CurrentBitPosition(size_t total_bytes, const BitReader& br) {
  return total_bytes * 8 - br.numBitsLeft();
}

bool CopyBits(const uint8_t* data,
              size_t size,
              size_t bit_offset,
              size_t bit_length,
              std::vector<uint8_t>* out) {
  out->assign((bit_length + 7) / 8, 0);
  if (bit_length == 0) {
    return true;
  }

  BitReader br(data, size);
  if (!br.skipBits(bit_offset)) {
    return false;
  }

  for (size_t i = 0; i < bit_length; ++i) {
    uint32_t bit = 0;
    if (!br.getBitsGraceful(1, &bit)) {
      return false;
    }
    if (bit != 0) {
      (*out)[i / 8] |= 1u << (7 - (i % 8));
    }
  }

  return true;
}

std::vector<uint8_t> MakeAacAudioSpecificConfig(uint8_t audio_object_type,
                                                uint8_t sampling_freq_index,
                                                uint8_t channel_config) {
  return {static_cast<uint8_t>((audio_object_type << 3) |
                               (sampling_freq_index >> 1)),
          static_cast<uint8_t>(((sampling_freq_index & 0x01) << 7) |
                               (channel_config << 3))};
}

bool ReadLatmValue(BitReader* br, uint64_t* value) {
  uint32_t bytes_for_value = 0;
  if (!br->getBitsGraceful(2, &bytes_for_value)) {
    return false;
  }

  uint64_t result = 0;
  for (uint32_t i = 0; i <= bytes_for_value; ++i) {
    uint32_t byte = 0;
    if (!br->getBitsGraceful(8, &byte)) {
      return false;
    }
    result = (result << 8) | byte;
  }

  *value = result;
  return true;
}

bool ReadAudioObjectType(BitReader* br, uint32_t* audio_object_type) {
  uint32_t value = 0;
  if (!br->getBitsGraceful(5, &value)) {
    return false;
  }

  if (value == 31) {
    uint32_t extension = 0;
    if (!br->getBitsGraceful(6, &extension)) {
      return false;
    }
    value = 32 + extension;
  }

  *audio_object_type = value;
  return true;
}

bool ReadGASpecificConfig(BitReader* br,
                          uint32_t audio_object_type,
                          uint32_t channel_config) {
  uint32_t frame_length_flag = 0;
  if (!br->getBitsGraceful(1, &frame_length_flag)) {
    return false;
  }

  uint32_t depends_on_core_coder = 0;
  if (!br->getBitsGraceful(1, &depends_on_core_coder)) {
    return false;
  }
  if (depends_on_core_coder != 0 && !br->skipBits(14)) {
    return false;
  }

  uint32_t extension_flag = 0;
  if (!br->getBitsGraceful(1, &extension_flag)) {
    return false;
  }

  if (channel_config == 0) {
    return false;
  }

  if ((audio_object_type == 6 || audio_object_type == 20) && !br->skipBits(3)) {
    return false;
  }

  if (extension_flag != 0) {
    if (audio_object_type == 22 && !br->skipBits(16)) {
      return false;
    }

    if ((audio_object_type == 17 || audio_object_type == 19 ||
         audio_object_type == 20 || audio_object_type == 23) &&
        !br->skipBits(3 + 1 + 1)) {
      return false;
    }

    if (!br->skipBits(1)) {
      return false;
    }
  }

  return true;
}

bool ParseAudioSpecificConfig(BitReader* br,
                              const uint8_t* data,
                              size_t size,
                              std::vector<uint8_t>* asc,
                              uint32_t* sample_rate,
                              uint8_t* channel_count) {
  const size_t asc_start = CurrentBitPosition(size, *br);

  uint32_t audio_object_type = 0;
  if (!ReadAudioObjectType(br, &audio_object_type)) {
    return false;
  }

  uint32_t sampling_frequency_index = 0;
  if (!br->getBitsGraceful(4, &sampling_frequency_index)) {
    return false;
  }

  uint32_t parsed_sample_rate = 0;
  if (sampling_frequency_index == 0x0f) {
    if (!br->getBitsGraceful(24, &parsed_sample_rate)) {
      return false;
    }
  } else {
    parsed_sample_rate = GetSamplingRate(sampling_frequency_index);
    if (parsed_sample_rate == 0) {
      return false;
    }
  }

  uint32_t channel_config = 0;
  if (!br->getBitsGraceful(4, &channel_config)) {
    return false;
  }

  if (audio_object_type == 5 || audio_object_type == 29) {
    uint32_t extension_frequency_index = 0;
    if (!br->getBitsGraceful(4, &extension_frequency_index)) {
      return false;
    }
    uint32_t extension_sample_rate = 0;
    if (extension_frequency_index == 0x0f) {
      if (!br->getBitsGraceful(24, &extension_sample_rate)) {
        return false;
      }
    } else {
      extension_sample_rate = GetSamplingRate(extension_frequency_index);
      if (extension_sample_rate == 0) {
        return false;
      }
    }
    parsed_sample_rate = extension_sample_rate;

    if (!ReadAudioObjectType(br, &audio_object_type)) {
      return false;
    }
    if (audio_object_type == 22 && !br->getBitsGraceful(4, &channel_config)) {
      return false;
    }
  }

  if (!ReadGASpecificConfig(br, audio_object_type, channel_config)) {
    return false;
  }

  const size_t asc_end = CurrentBitPosition(size, *br);
  if (!CopyBits(data, size, asc_start, asc_end - asc_start, asc)) {
    return false;
  }

  uint8_t parsed_channel_count = GetChannelCount(channel_config);
  if (parsed_channel_count == 0) {
    return false;
  }

  *sample_rate = parsed_sample_rate;
  *channel_count = parsed_channel_count;
  return true;
}

bool ParseLatmStreamMuxConfig(BitReader* br,
                              const uint8_t* data,
                              size_t size,
                              LatmStreamMuxConfig* config) {
  uint32_t audio_mux_version = 0;
  if (!br->getBitsGraceful(1, &audio_mux_version)) {
    return false;
  }

  uint32_t audio_mux_version_a = 0;
  if (audio_mux_version != 0 && !br->getBitsGraceful(1, &audio_mux_version_a)) {
    return false;
  }

  if (audio_mux_version_a != 0) {
    return false;
  }

  if (audio_mux_version != 0) {
    uint64_t tara_buffer_fullness = 0;
    if (!ReadLatmValue(br, &tara_buffer_fullness)) {
      return false;
    }
  }

  uint32_t all_streams_same_time_framing = 0;
  if (!br->getBitsGraceful(1, &all_streams_same_time_framing) ||
      all_streams_same_time_framing == 0) {
    return false;
  }

  if (!br->getBitsGraceful(6, &config->num_subframes) ||
      !br->getBitsGraceful(4, &audio_mux_version_a) ||
      audio_mux_version_a != 0) {
    return false;
  }

  uint32_t num_layer = 0;
  if (!br->getBitsGraceful(3, &num_layer) || num_layer != 0) {
    return false;
  }

  if (audio_mux_version != 0) {
    return false;
  }

  if (!ParseAudioSpecificConfig(br, data, size, &config->audio_specific_config,
                                &config->sample_rate, &config->channel_count)) {
    return false;
  }

  if (!br->getBitsGraceful(3, &config->frame_length_type)) {
    return false;
  }

  switch (config->frame_length_type) {
    case 0:
      if (!br->skipBits(8)) {
        return false;
      }
      break;
    case 1:
      if (!br->skipBits(9)) {
        return false;
      }
      break;
    case 3:
    case 4:
    case 5:
      if (!br->skipBits(6)) {
        return false;
      }
      break;
    case 6:
    case 7:
      if (!br->skipBits(1)) {
        return false;
      }
      break;
    default:
      return false;
  }

  uint32_t other_data_present = 0;
  if (!br->getBitsGraceful(1, &other_data_present)) {
    return false;
  }
  config->other_data_present = other_data_present != 0;
  config->other_data_len_bits = 0;

  if (config->other_data_present) {
    do {
      uint32_t esc = 0;
      uint32_t value = 0;
      if (!br->getBitsGraceful(1, &esc) || !br->getBitsGraceful(8, &value)) {
        return false;
      }
      config->other_data_len_bits = (config->other_data_len_bits << 8) | value;
      if (esc == 0) {
        break;
      }
    } while (true);
  }

  uint32_t crc_check_present = 0;
  if (!br->getBitsGraceful(1, &crc_check_present)) {
    return false;
  }
  if (crc_check_present != 0 && !br->skipBits(8)) {
    return false;
  }

  return true;
}

bool ReadLatmPayloadLength(BitReader* br, size_t* payload_length) {
  size_t length = 0;
  while (true) {
    uint32_t tmp = 0;
    if (!br->getBitsGraceful(8, &tmp)) {
      return false;
    }
    length += tmp;
    if (tmp != 255) {
      break;
    }
  }

  *payload_length = length;
  return true;
}

}  // namespace

ESQueue::ESQueue(Mode mode, uint32_t flags)
    : mode_(mode), flags_(flags), eos_reached_(false), ca_system_id_(0) {
  AVE_LOG(LS_INFO) << "ESQueue mode=" << static_cast<int>(mode)
                   << " flags=" << flags << " is_scrambled=" << IsScrambled()
                   << " is_sample_encrypted=" << IsSampleEncrypted();
}

ESQueue::~ESQueue() = default;

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
    avc_sps_.reset();
    avc_pps_.reset();
    latm_stream_mux_read_ = false;
    latm_num_subframes_ = 0;
    latm_frame_length_type_ = 0;
    latm_other_data_present_ = false;
    latm_other_data_len_bits_ = 0;
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
  const size_t current_size = buffer_ != nullptr ? buffer_->size() : 0;

  if (buffer_ == nullptr || current_size + size > buffer_->capacity()) {
    size_t new_capacity = buffer_ != nullptr ? buffer_->capacity() : 0;

    while (new_capacity < current_size + size) {
      if (new_capacity < 8192) {
        new_capacity = 8192;
      } else if (new_capacity < 2 * 1024 * 1024) {
        new_capacity *= 2;
      } else {
        new_capacity += 1024 * 1024;
      }
    }

    auto new_buffer = std::make_shared<Buffer>(new_capacity);
    new_buffer->setRange(0, current_size);
    if (buffer_ != nullptr) {
      memcpy(new_buffer->data(), buffer_->data(), current_size);
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

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnit(bool force_flush) {
  if ((flags_ & kFlag_AlignedData) != 0) {
    if (range_infos_.empty()) {
      return nullptr;
    }

    RangeInfo info = range_infos_.front();
    range_infos_.pop_front();

    auto access_unit = MediaFrame::CreateSharedAsCopy(
        buffer_->data(), info.length_,
        (mode_ == Mode::H264 || mode_ == Mode::HEVC ||
         mode_ == Mode::MPEG_VIDEO || mode_ == Mode::MPEG4_VIDEO)
            ? MediaType::VIDEO
            : MediaType::AUDIO);

    if (info.timestamp_us_ >= 0) {
      access_unit->SetPts(base::Timestamp::Micros(info.timestamp_us_));
    }

    memmove(buffer_->data(), buffer_->data() + info.length_,
            buffer_->size() - info.length_);
    buffer_->setRange(0, buffer_->size() - info.length_);

    return access_unit;
  }

  switch (mode_) {
    case Mode::H264:
      return DequeueAccessUnitH264(force_flush);
    case Mode::HEVC:
      return DequeueAccessUnitHEVC(force_flush);
    case Mode::AAC:
      return DequeueAccessUnitAAC();
    case Mode::LATM:
      return DequeueAccessUnitLATM();
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
      AVE_LOG(LS_ERROR) << "Unknown mode: " << static_cast<int>(mode_);
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

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitH264(bool force_flush) {
  if (buffer_ == nullptr || buffer_->size() == 0) {
    return nullptr;
  }

  const uint8_t* const buffer_data = buffer_->data();
  const size_t buffer_size = buffer_->size();

  const uint8_t* scan = buffer_data;
  size_t remaining = buffer_size;
  std::vector<H264NalUnit> nal_units;
  bool have_vcl = false;
  bool has_idr = false;

  auto maybe_update_format = [&]() {
    if (!avc_sps_ || !avc_pps_) {
      return;
    }

    std::vector<uint8_t> codec_config;
    codec_config.reserve(sizeof(kStartCode) * 2 + avc_sps_->size() +
                         avc_pps_->size());
    codec_config.insert(codec_config.end(), std::begin(kStartCode),
                        std::end(kStartCode));
    codec_config.insert(codec_config.end(), avc_sps_->data(),
                        avc_sps_->data() + avc_sps_->size());
    codec_config.insert(codec_config.end(), std::begin(kStartCode),
                        std::end(kStartCode));
    codec_config.insert(codec_config.end(), avc_pps_->data(),
                        avc_pps_->data() + avc_pps_->size());

    int32_t width = 0;
    int32_t height = 0;
    int32_t sar_width = 0;
    int32_t sar_height = 0;
    auto csd = MakeAVCCodecSpecificData(
        Buffer::CreateAsCopy(codec_config.data(), codec_config.size()), &width,
        &height, &sar_width, &sar_height);
    if (!csd) {
      return;
    }

    if (!format_) {
      format_ =
          MediaMeta::CreatePtr(MediaType::VIDEO, MediaMeta::FormatType::kTrack);
    }

    format_->SetMime(MEDIA_MIMETYPE_VIDEO_AVC);
    format_->SetPrivateData(static_cast<uint32_t>(csd->size()), csd->data());
    if (width > 0) {
      format_->SetWidth(width);
    }
    if (height > 0) {
      format_->SetHeight(height);
    }
    if (sar_width > 0 && sar_height > 0) {
      format_->SetSampleAspectRatio(
          {static_cast<int16_t>(sar_width), static_cast<int16_t>(sar_height)});
    }
    AVE_LOG(LS_INFO) << "ESQueue H264 format ready: width=" << format_->width()
                     << " height=" << format_->height()
                     << " csd_size=" << csd->size();
  };

  auto build_access_unit =
      [&](size_t consumed_size) -> std::shared_ptr<MediaFrame> {
    if (nal_units.empty()) {
      return nullptr;
    }

    bool has_i_slice = false;
    for (const auto& nal : nal_units) {
      if (!IsH264VclNal(nal.type)) {
        continue;
      }
      uint32_t slice_type = 0;
      if (ParseH264SliceType(nal.data, nal.size, &slice_type) &&
          IsH264ISliceType(slice_type)) {
        has_i_slice = true;
      }
      break;
    }

    size_t access_unit_size = 0;
    for (const auto& nal : nal_units) {
      access_unit_size += sizeof(kStartCode) + nal.size;
    }

    auto access_unit =
        MediaFrame::CreateShared(access_unit_size, MediaType::VIDEO);
    uint8_t* out = access_unit->data();
    for (const auto& nal : nal_units) {
      memcpy(out, kStartCode, sizeof(kStartCode));
      out += sizeof(kStartCode);
      memcpy(out, nal.data, nal.size);
      out += nal.size;
    }
    access_unit->setRange(0, access_unit_size);

    int64_t time_us = FetchTimestamp(consumed_size);
    if (time_us >= 0) {
      access_unit->SetPts(base::Timestamp::Micros(time_us));
    }

    memmove(buffer_->data(), buffer_->data() + consumed_size,
            buffer_size - consumed_size);
    buffer_->setRange(0, buffer_size - consumed_size);

    maybe_update_format();

    access_unit->SetMime(MEDIA_MIMETYPE_VIDEO_AVC);
    if (format_) {
      if (format_->width() > 0) {
        access_unit->SetWidth(format_->width());
      }
      if (format_->height() > 0) {
        access_unit->SetHeight(format_->height());
      }
    }

    if (has_idr || has_i_slice) {
      access_unit->SetPictureType(PictureType::I);
    }

    AVE_LOG(LS_INFO) << "ESQueue H264 access unit: nal_count="
                     << nal_units.size() << " size=" << access_unit_size
                     << " has_idr=" << has_idr << " has_i_slice=" << has_i_slice
                     << " format_ready=" << static_cast<bool>(format_);

    return access_unit;
  };

  while (true) {
    const uint8_t* nal_scan = scan;
    const uint8_t* nal_start = nullptr;
    size_t nal_size = 0;
    status_t err =
        getNextNALUnit(&scan, &remaining, &nal_start, &nal_size, true);
    if (err != OK) {
      if ((force_flush || eos_reached_) && !nal_units.empty() && have_vcl) {
        return build_access_unit(buffer_size);
      }
      return nullptr;
    }

    if (nal_size == 0) {
      continue;
    }

    const unsigned nal_type = nal_start[0] & 0x1f;
    const size_t boundary_offset = static_cast<size_t>(nal_scan - buffer_data);
    bool access_unit_boundary = StartsNewH264AccessUnit(nal_type, have_vcl);

    if (!access_unit_boundary && IsH264VclNal(nal_type) && have_vcl) {
      access_unit_boundary = IsFirstH264SliceInAccessUnit(nal_start, nal_size);
    }

    if (access_unit_boundary && !nal_units.empty()) {
      return build_access_unit(boundary_offset);
    }

    nal_units.push_back(H264NalUnit{nal_start, nal_size, nal_type});
    if (nal_type == 7) {
      avc_sps_ = Buffer::CreateAsCopy(nal_start, nal_size);
      AVE_LOG(LS_INFO) << "ESQueue H264 saw SPS size=" << nal_size;
    } else if (nal_type == 8) {
      avc_pps_ = Buffer::CreateAsCopy(nal_start, nal_size);
      AVE_LOG(LS_INFO) << "ESQueue H264 saw PPS size=" << nal_size;
    }

    if (IsH264VclNal(nal_type)) {
      have_vcl = true;
      has_idr = has_idr || nal_type == 5;
    }
  }
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitHEVC(bool force_flush) {
  return DequeueAccessUnitH264(force_flush);
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitAAC() {
  if (buffer_ == nullptr || buffer_->size() < 7) {
    return nullptr;
  }

  const uint8_t* data = buffer_->data();
  size_t size = buffer_->size();

  size_t offset = 0;
  while (offset + 7 <= size) {
    if ((data[offset] == 0xFF) && ((data[offset + 1] & 0xF6) == 0xF0)) {
      size_t frame_length = ((data[offset + 3] & 0x03) << 11) |
                            (data[offset + 4] << 3) |
                            ((data[offset + 5] & 0xE0) >> 5);

      if (offset + frame_length > size) {
        return nullptr;
      }

      auto access_unit =
          CreateFrameCopy(data + offset, frame_length, MediaType::AUDIO);

      int64_t time_us = FetchTimestamp(frame_length + offset);
      if (time_us >= 0) {
        access_unit->SetPts(base::Timestamp::Micros(time_us));
      }

      memmove(buffer_->data(), buffer_->data() + offset + frame_length,
              size - offset - frame_length);
      buffer_->setRange(0, size - offset - frame_length);

      ADTSHeader header{};
      if (ParseADTSHeader(data + offset, frame_length, &header) != OK) {
        return access_unit;
      }

      if (!format_) {
        format_ = MediaMeta::CreatePtr(MediaType::AUDIO,
                                       MediaMeta::FormatType::kTrack);
      }

      format_->SetMime(MEDIA_MIMETYPE_AUDIO_AAC);
      access_unit->SetMime(MEDIA_MIMETYPE_AUDIO_AAC);

      const uint32_t sample_rate = GetSamplingRate(header.sampling_freq_index);
      if (sample_rate > 0) {
        format_->SetSampleRate(sample_rate);
        access_unit->SetSampleRate(sample_rate);
      }

      const uint8_t channel_count = GetChannelCount(header.channel_config);
      if (channel_count > 0) {
        const ChannelLayout layout = GuessChannelLayout(channel_count);
        if (layout != CHANNEL_LAYOUT_UNSUPPORTED) {
          format_->SetChannelLayout(layout);
          access_unit->SetChannelLayout(layout);
        }
      }

      auto asc = MakeAacAudioSpecificConfig(
          static_cast<uint8_t>(header.profile + 1), header.sampling_freq_index,
          header.channel_config);
      format_->SetPrivateData(static_cast<uint32_t>(asc.size()), asc.data());

      return access_unit;
    }
    ++offset;
  }

  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitLATM() {
  if (buffer_ == nullptr || buffer_->size() < 3) {
    return nullptr;
  }

  const uint8_t* data = buffer_->data();
  const size_t size = buffer_->size();

  size_t offset = 0;
  while (offset + 3 <= size) {
    if (data[offset] != 0x56 || (data[offset + 1] & 0xE0) != 0xE0) {
      ++offset;
      continue;
    }

    const size_t mux_length_bytes =
        static_cast<size_t>((data[offset + 1] & 0x1F) << 8) | data[offset + 2];
    const size_t frame_size = 3 + mux_length_bytes;
    if (offset + frame_size > size) {
      return nullptr;
    }

    const uint8_t* frame_data = data + offset + 3;
    BitReader br(frame_data, mux_length_bytes);

    uint32_t use_same_stream_mux = 0;
    if (!br.getBitsGraceful(1, &use_same_stream_mux)) {
      ++offset;
      continue;
    }

    if (use_same_stream_mux == 0) {
      LatmStreamMuxConfig config;
      if (!ParseLatmStreamMuxConfig(&br, frame_data, mux_length_bytes,
                                    &config)) {
        ++offset;
        continue;
      }

      if (config.num_subframes != 0 || config.frame_length_type != 0 ||
          config.audio_specific_config.empty()) {
        ++offset;
        continue;
      }

      latm_stream_mux_read_ = true;
      latm_num_subframes_ = config.num_subframes;
      latm_frame_length_type_ = config.frame_length_type;
      latm_other_data_present_ = config.other_data_present;
      latm_other_data_len_bits_ = config.other_data_len_bits;

      if (!format_) {
        format_ = MediaMeta::CreatePtr(MediaType::AUDIO,
                                       MediaMeta::FormatType::kTrack);
      }

      format_->SetMime(MEDIA_MIMETYPE_AUDIO_AAC);
      format_->SetSampleRate(config.sample_rate);
      const ChannelLayout layout = GuessChannelLayout(config.channel_count);
      if (layout != CHANNEL_LAYOUT_UNSUPPORTED) {
        format_->SetChannelLayout(layout);
      }
      format_->SetPrivateData(
          static_cast<uint32_t>(config.audio_specific_config.size()),
          config.audio_specific_config.data());
      AVE_LOG(LS_INFO) << "ESQueue LATM format ready: sample_rate="
                       << format_->sample_rate() << " channel_layout="
                       << static_cast<int>(format_->channel_layout())
                       << " asc_size=" << config.audio_specific_config.size();
    } else if (!latm_stream_mux_read_) {
      ++offset;
      continue;
    }

    if (latm_num_subframes_ != 0 || latm_frame_length_type_ != 0 || !format_) {
      ++offset;
      continue;
    }

    size_t payload_length = 0;
    if (!ReadLatmPayloadLength(&br, &payload_length)) {
      return nullptr;
    }

    const size_t payload_bit_offset = CurrentBitPosition(mux_length_bytes, br);
    std::vector<uint8_t> payload;
    if (!CopyBits(frame_data, mux_length_bytes, payload_bit_offset,
                  payload_length * 8, &payload)) {
      ++offset;
      continue;
    }

    if (!br.skipBits(payload_length * 8)) {
      ++offset;
      continue;
    }

    if (latm_other_data_present_ &&
        !br.skipBits(static_cast<size_t>(latm_other_data_len_bits_))) {
      ++offset;
      continue;
    }

    auto access_unit =
        CreateFrameCopy(payload.data(), payload.size(), MediaType::AUDIO);
    int64_t time_us = FetchTimestamp(offset + frame_size);
    if (time_us >= 0) {
      access_unit->SetPts(base::Timestamp::Micros(time_us));
    }

    memmove(buffer_->data(), buffer_->data() + offset + frame_size,
            size - offset - frame_size);
    buffer_->setRange(0, size - offset - frame_size);

    access_unit->SetMime(MEDIA_MIMETYPE_AUDIO_AAC);
    access_unit->SetSampleRate(format_->sample_rate());
    access_unit->SetChannelLayout(format_->channel_layout());
    return access_unit;
  }

  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitAC3() {
  AVE_LOG(LS_WARNING) << "AC3 dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitEAC3() {
  AVE_LOG(LS_WARNING) << "EAC3 dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitAC4() {
  AVE_LOG(LS_WARNING) << "AC4 dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitMPEGAudio() {
  AVE_LOG(LS_WARNING) << "MPEG Audio dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitMPEGVideo() {
  AVE_LOG(LS_WARNING) << "MPEG Video dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitMPEG4Video() {
  AVE_LOG(LS_WARNING) << "MPEG4 Video dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitPCMAudio() {
  AVE_LOG(LS_WARNING) << "PCM Audio dequeue not fully implemented";
  return nullptr;
}

std::shared_ptr<MediaFrame> ESQueue::DequeueAccessUnitMetadata() {
  AVE_LOG(LS_WARNING) << "Metadata dequeue not fully implemented";
  return nullptr;
}

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave
