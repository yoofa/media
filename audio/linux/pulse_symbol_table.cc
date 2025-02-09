/*
 * pulse_symbol_table.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/pulse_symbol_table.h"

namespace ave {
namespace media {
namespace linux_audio {

// NOLINTBEGIN(modernize-avoid-c-arrays)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(PulseAudioSymbolTable, "libpulse.so.0")
// NOLINTEND(modernize-avoid-c-arrays)
#define X(sym) \
  LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(PulseAudioSymbolTable, sym)
PULSE_AUDIO_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(PulseAudioSymbolTable)

}  // namespace linux_audio
}  // namespace media
}  // namespace ave
