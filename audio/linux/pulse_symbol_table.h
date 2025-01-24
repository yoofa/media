/*
 * pulse_symbol_table.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_LINUX_PULSE_SYMBOL_TABLE_H_
#define AVE_MEDIA_AUDIO_LINUX_PULSE_SYMBOL_TABLE_H_

#include "media/audio/linux/latebinding_symbol_table.h"

// 在namespace外部声明获取符号表实例的函数
ave::media::linux::PulseSymbolTable* GetPulseSymbolTable();

namespace ave {
namespace media {
namespace linux {

// PulseAudio symbols we need
#define PULSE_SYMBOLS_LIST            \
  X(pa_threaded_mainloop_new)         \
  X(pa_threaded_mainloop_free)        \
  X(pa_threaded_mainloop_start)       \
  X(pa_threaded_mainloop_stop)        \
  X(pa_threaded_mainloop_lock)        \
  X(pa_threaded_mainloop_unlock)      \
  X(pa_threaded_mainloop_wait)        \
  X(pa_threaded_mainloop_signal)      \
  X(pa_threaded_mainloop_get_api)     \
  X(pa_context_new)                   \
  X(pa_context_unref)                 \
  X(pa_context_connect)               \
  X(pa_context_disconnect)            \
  X(pa_context_get_state)             \
  X(pa_context_get_sink_info_list)    \
  X(pa_context_get_source_info_list)  \
  X(pa_context_set_state_callback)    \
  X(pa_stream_new)                    \
  X(pa_stream_unref)                  \
  X(pa_stream_connect_playback)       \
  X(pa_stream_connect_record)         \
  X(pa_stream_disconnect)             \
  X(pa_stream_get_state)              \
  X(pa_stream_get_latency)            \
  X(pa_stream_writable_size)          \
  X(pa_stream_write)                  \
  X(pa_stream_set_write_callback)     \
  X(pa_stream_set_state_callback)     \
  X(pa_stream_set_underflow_callback) \
  X(pa_strerror)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(PulseSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(PulseSymbolTable, sym)
PULSE_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(PulseSymbolTable)

}  // namespace linux
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_AUDIO_LINUX_PULSE_SYMBOL_TABLE_H_