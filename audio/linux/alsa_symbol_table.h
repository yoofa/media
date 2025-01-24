/*
 * alsa_symbol_table.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_LINUX_ALSA_SYMBOL_TABLE_H_
#define AVE_MEDIA_AUDIO_LINUX_ALSA_SYMBOL_TABLE_H_

#include "media/audio/linux/latebinding_symbol_table.h"

namespace ave {
namespace media {
namespace linux {

// ALSA symbols we need
#define ALSA_SYMBOLS_LIST                   \
  X(snd_pcm_open)                           \
  X(snd_pcm_close)                          \
  X(snd_pcm_prepare)                        \
  X(snd_pcm_start)                          \
  X(snd_pcm_drop)                           \
  X(snd_pcm_drain)                          \
  X(snd_pcm_pause)                          \
  X(snd_pcm_resume)                         \
  X(snd_pcm_wait)                           \
  X(snd_pcm_writei)                         \
  X(snd_pcm_readi)                          \
  X(snd_pcm_avail)                          \
  X(snd_pcm_avail_update)                   \
  X(snd_pcm_state)                          \
  X(snd_pcm_hw_params)                      \
  X(snd_pcm_hw_params_any)                  \
  X(snd_pcm_hw_params_set_access)           \
  X(snd_pcm_hw_params_set_format)           \
  X(snd_pcm_hw_params_set_channels)         \
  X(snd_pcm_hw_params_set_rate_near)        \
  X(snd_pcm_hw_params_set_period_size_near) \
  X(snd_pcm_hw_params_set_buffer_size_near) \
  X(snd_pcm_hw_params_get_buffer_size)      \
  X(snd_pcm_hw_params_get_period_size)      \
  X(snd_pcm_sw_params)                      \
  X(snd_pcm_sw_params_current)              \
  X(snd_pcm_sw_params_set_start_threshold)  \
  X(snd_pcm_sw_params_set_avail_min)        \
  X(snd_pcm_name)                           \
  X(snd_strerror)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(AlsaSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(AlsaSymbolTable, sym)
ALSA_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(AlsaSymbolTable)

}  // namespace linux
}  // namespace media
}  // namespace ave

// 在namespace外部添加
ave::media::linux::AlsaSymbolTable* GetAlsaSymbolTable();

#endif  // AVE_MEDIA_AUDIO_LINUX_ALSA_SYMBOL_TABLE_H_