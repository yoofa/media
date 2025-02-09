/*
 * alsa_symbol_table.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/alsa_symbol_table.h"

ave::media::linux_audio::AlsaSymbolTable* GetAlsaSymbolTable() {
  static auto* symbol_table = new ave::media::linux_audio::AlsaSymbolTable();
  return symbol_table;
}

namespace ave {
namespace media {
namespace linux_audio {

// NOLINTBEGIN(modernize-avoid-c-arrays)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(AlsaSymbolTable, "libasound.so.2")
// NOLINTEND(modernize-avoid-c-arrays)

#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(AlsaSymbolTable, sym)
ALSA_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(AlsaSymbolTable)

}  // namespace linux_audio
}  // namespace media
}  // namespace ave
