/*
 * aac_utils.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "aac_utils.h"
#include "base/logging.h"

#include <array>

namespace ave {
namespace media {

//// AAC sampling frequencies (indexed by sampling_freq_index)
// static const uint32_t kSamplingRates[] = {
//     96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
//     16000, 12000, 11025, 8000,  7350,  0,     0,     0};

namespace {
// AAC sampling frequencies (indexed by sampling_freq_index)
constexpr std::array<uint32_t, 16> kSamplingRates = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,  7350,  0,     0,     0};

constexpr std::array<uint8_t, 8> kChannelCounts = {0, 1, 2, 3, 4, 5, 6, 8};

}  // namespace

status_t ParseADTSHeader(const uint8_t* data, size_t size, ADTSHeader* header) {
  if (size < 7) {
    return E_AGAIN;  // Need at least 7 bytes for ADTS header
  }

  // Check syncword (0xFFF)
  if (data[0] != 0xFF || (data[1] & 0xF0) != 0xF0) {
    return INVALID_OPERATION;  // Invalid ADTS header
  }

  // Parse ADTS header fields
  // data[1]: 1111BCCD
  //   B: MPEG Version (0 = MPEG-4, 1 = MPEG-2)
  //   CC: Layer (always 00)
  //   D: protection_absent
  header->protection_absent = (data[1] & 0x01) != 0;

  // data[2]: AABBCCCC
  //   AA: profile (MPEG-4 Audio Object Type - 1)
  //   BB: sampling_frequency_index (bits 3-2)
  //   C: private bit
  //   CCC: sampling_frequency_index (bits 1-0) and channel_config (bit 2)
  header->profile = (data[2] >> 6) & 0x03;
  header->sampling_freq_index = (data[2] >> 2) & 0x0F;
  header->channel_config = ((data[2] & 0x01) << 2) | ((data[3] >> 6) & 0x03);

  // data[3-5]: Parse frame length (13 bits)
  // data[3]: AABCCCCC
  //   AA: channel_config (bits 1-0)
  //   B: originality
  //   CCCCC: frame_length (bits 12-8)
  // data[4]: AAAAAAAA (frame_length bits 7-0)
  // data[5]: CCCBBBBB
  //   CCC: frame_length (bits 2-0)
  //   BBBBB: buffer_fullness (bits 10-6)
  header->frame_length =
      ((data[3] & 0x03) << 11) | (data[4] << 3) | ((data[5] >> 5) & 0x07);

  if (header->frame_length < 7) {
    AVE_LOG(LS_ERROR) << "Invalid ADTS frame length: " << header->frame_length;
    return INVALID_OPERATION;
  }

  return OK;
}

status_t GetNextAACFrame(const uint8_t** _data,
                         size_t* _size,
                         const uint8_t** frameStart,
                         size_t* frameSize) {
  const uint8_t* data = *_data;
  size_t size = *_size;

  *frameStart = nullptr;
  *frameSize = 0;

  if (size < 7) {
    return E_AGAIN;
  }

  // Find ADTS sync word (0xFFF)
  size_t offset = 0;
  while (offset < size - 1) {
    if (data[offset] == 0xFF && (data[offset + 1] & 0xF0) == 0xF0) {
      break;
    }
    offset++;
  }

  if (offset >= size - 1) {
    *_data = data + size;
    *_size = 0;
    return E_AGAIN;
  }

  // Parse ADTS header
  ADTSHeader header{};
  status_t result = ParseADTSHeader(data + offset, size - offset, &header);
  if (result != OK) {
    if (result == E_AGAIN) {
      *_data = data + offset;
      *_size = size - offset;
      return E_AGAIN;
    }
    // Invalid header, skip this byte and continue searching
    *_data = data + offset + 1;
    *_size = size - offset - 1;
    return INVALID_OPERATION;
  }

  // Check if we have the complete frame
  if (size - offset < header.frame_length) {
    *_data = data + offset;
    *_size = size - offset;
    return E_AGAIN;
  }

  // We have a complete frame
  *frameStart = data + offset;
  *frameSize = header.frame_length;
  *_data = data + offset + header.frame_length;
  *_size = size - offset - header.frame_length;

  return OK;
}

uint32_t GetSamplingRate(uint8_t sampling_freq_index) {
  if (sampling_freq_index >= kSamplingRates.size()) {
    return 0;
  }

  return kSamplingRates[sampling_freq_index];
}

uint8_t GetChannelCount(uint8_t channel_config) {
  // AAC channel configurations:
  // 0: defined in AOT specific config
  // 1: 1 channel: front-center
  // 2: 2 channels: front-left, front-right
  // 3: 3 channels: front-center, front-left, front-right
  // 4: 4 channels: front-center, front-left, front-right, back-center
  // 5: 5 channels: front-center, front-left, front-right, back-left, back-right
  // 6: 6 channels: front-center, front-left, front-right, back-left,
  // back-right, LFE-channel 7: 8 channels: front-center, front-left,
  // front-right, side-left, side-right, back-left, back-right, LFE-channel
  if (channel_config >= kChannelCounts.size()) {
    return 0;
  }
  return kChannelCounts[channel_config];
}

}  // namespace media
}  // namespace ave
