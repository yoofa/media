/*
 * aac_utils.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AAC_UTILS_H
#define AAC_UTILS_H

#include <cstddef>
#include <cstdint>
#include "base/errors.h"

namespace ave {
namespace media {

// AAC ADTS header structure
struct ADTSHeader {
  uint8_t profile;              // 2 bits: MPEG-4 Audio Object Type minus 1
  uint8_t sampling_freq_index;  // 4 bits
  uint8_t channel_config;       // 3 bits
  uint16_t frame_length;        // 13 bits: length including header
  bool protection_absent;       // 1 bit
};

// Parse ADTS header from data
// Returns OK if successful, error otherwise
status_t ParseADTSHeader(const uint8_t* data, size_t size, ADTSHeader* header);

// Get the next AAC frame from the data buffer
// Updates _data and _size to point to remaining data after the frame
// Sets frameStart to point to the frame data (including ADTS header)
// Sets frameSize to the total frame size (including ADTS header)
// Returns OK if a complete frame is found, E_AGAIN if more data is needed
status_t GetNextAACFrame(const uint8_t** _data,
                         size_t* _size,
                         const uint8_t** frameStart,
                         size_t* frameSize);

// Get sampling rate from sampling frequency index
uint32_t GetSamplingRate(uint8_t sampling_freq_index);

// Get channel count from channel configuration
uint8_t GetChannelCount(uint8_t channel_config);

}  // namespace media
}  // namespace ave

#endif /* !AAC_UTILS_H */
