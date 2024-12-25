/*
 * audio.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_H
#define AVE_MEDIA_AUDIO_H

#include "audio_format.h"
#include "channel_layout.h"

namespace ave {
namespace media {

// from android system/media/audio/include/system/audio.h
using audio_offload_info_t = struct AudioOffloadInfo {
  uint16_t version = 0;                      // version of the info structure
  uint16_t size = sizeof(AudioOffloadInfo);  // total size of the structure
                                             // including version and size
  uint32_t sample_rate = 0;                  // sample rate in Hz
  channel_layout_t channel_layout = CHANNEL_LAYOUT_STEREO;  // channel layout
  audio_format_t format = AUDIO_FORMAT_DEFAULT;             // audio format
  uint32_t bit_rate = 0;      // bit rate in bits per second
  int64_t duration_us = 0;    // duration in microseconds, -1 if unknown
  bool has_video = false;     // true if stream is tied to a video stream
  bool is_streaming = false;  // true if streaming, false if local playback
  uint32_t bit_width = 16;    // number of bits per sample
  uint32_t offload_buffer_size = 0;  // offload fragment size
  int32_t content_id = 0;  // version 0.2: content id from tuner hal (0 if none)
  int32_t sync_id = 0;     // version 0.2: sync id from tuner hal (0 if none)
} __attribute__((aligned(8)));

using audio_config_t = struct AudioConfig {
  uint32_t sample_rate = 0;                                 // sample rate in Hz
  channel_layout_t channel_layout = CHANNEL_LAYOUT_STEREO;  // channel layout
  audio_format_t format = AUDIO_FORMAT_DEFAULT;             // audio format
  audio_offload_info_t offload_info;  // offload information
  uint32_t frame_size = 0;            // size of a single frame in bytes
} __attribute__((aligned(8)));

static const audio_config_t DefaultAudioConfig{
    .sample_rate = 44100,
    .channel_layout = CHANNEL_LAYOUT_STEREO,
    .format = AUDIO_FORMAT_PCM_16_BIT,
    .offload_info = {},
    .frame_size = 0,

};

}  // namespace media
}  // namespace ave
#endif /* !AVE_MEDIA_AUDIO_H */
