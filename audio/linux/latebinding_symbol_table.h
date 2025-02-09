/*
 * latebinding_symbol_table.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_AUDIO_LINUX_LATEBINDING_SYMBOL_TABLE_H_
#define AVE_MEDIA_AUDIO_LINUX_LATEBINDING_SYMBOL_TABLE_H_

#include <dlfcn.h>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace ave {
namespace media {

using DllHandle = void*;
const DllHandle kInvalidDllHandle = nullptr;

// Helper functions
DllHandle InternalLoadDll(std::string_view dll_name);

void InternalUnloadDll(DllHandle handle);

// NOLINTBEGIN(modernize-avoid-c-arrays)
bool InternalLoadSymbols(DllHandle handle,
                         int num_symbols,
                         const char* const symbol_names[],
                         void* symbols[]);

// Symbol table template class
template <int SYMBOL_TABLE_SIZE,
          const char kDllName[],
          const char* const kSymbolNames[]>
// NOLINTEND(modernize-avoid-c-arrays)
class LateBindingSymbolTable {
 public:
  LateBindingSymbolTable()
      : handle_(kInvalidDllHandle), undefined_symbols_(false), symbols_{} {
    memset(symbols_, 0, sizeof(symbols_));
  }

  ~LateBindingSymbolTable() { Unload(); }

  bool Load() {
    if (IsLoaded()) {
      return true;
    }

    if (undefined_symbols_) {
      // We do not attempt to load again because repeated attempts are not
      // likely to succeed and DLL loading is costly.
      return false;
    }

    handle_ = InternalLoadDll(kDllName);
    if (!IsLoaded()) {
      return false;
    }
    if (!InternalLoadSymbols(handle_, NumSymbols(), kSymbolNames, symbols_)) {
      undefined_symbols_ = true;
      Unload();
      return false;
    }
    return true;
  }

  void Unload() {
    if (IsLoaded()) {
      InternalUnloadDll(handle_);
      handle_ = kInvalidDllHandle;
      memset(symbols_, 0, sizeof(symbols_));
    }
  }

  bool IsLoaded() const { return handle_ != kInvalidDllHandle; }

  static int NumSymbols() { return SYMBOL_TABLE_SIZE; }

  void* GetSymbol(int index) const { return symbols_[index]; }

 private:
  DllHandle handle_;
  bool undefined_symbols_;
  // NOLINTBEGIN(modernize-avoid-c-arrays)
  void* symbols_[SYMBOL_TABLE_SIZE];
  // NOLINTEND(modernize-avoid-c-arrays)
};

// Macros for declaring symbol tables
#define LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(ClassName) enum {
#define LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(ClassName, sym) \
  ClassName##_SYMBOL_TABLE_INDEX_##sym,

// NOLINTBEGIN(modernize-avoid-c-arrays)
#define LATE_BINDING_SYMBOL_TABLE_DECLARE_END(ClassName)                  \
  ClassName##_SYMBOL_TABLE_SIZE                                           \
  }                                                                       \
  ;                                                                       \
                                                                          \
  extern const char ClassName##_kDllName[];                               \
  extern const char* const                                                \
      ClassName##_kSymbolNames[ClassName##_SYMBOL_TABLE_SIZE];            \
                                                                          \
  using ClassName =                                                       \
      ::ave::media::LateBindingSymbolTable<ClassName##_SYMBOL_TABLE_SIZE, \
                                           ClassName##_kDllName,          \
                                           ClassName##_kSymbolNames>(     \
          ClassName);
// NOLINTEND(modernize-avoid-c-arrays)

// Macros for defining symbol tables
#define LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(ClassName, dllName) \
  const char ClassName##_kDllName[] = dllName;                     \
  const char* const ClassName##_kSymbolNames[ClassName##_SYMBOL_TABLE_SIZE] = {
#define LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(ClassName, sym) #sym,

#define LATE_BINDING_SYMBOL_TABLE_DEFINE_END(ClassName) \
  }                                                     \
  ;

// Macros for accessing symbols
#define LATESYM_INDEXOF(ClassName, sym) (ClassName##_SYMBOL_TABLE_INDEX_##sym)

#define LATESYM_GET(ClassName, inst, sym) \
  (*reinterpret_cast<__typeof__(&(sym))>( \
      (inst)->GetSymbol(LATESYM_INDEXOF(ClassName, sym))))

}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_AUDIO_LINUX_LATEBINDING_SYMBOL_TABLE_H_
