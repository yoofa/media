/*
 * latebinding_symbol_table.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/linux/latebinding_symbol_table.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace ave {
namespace media {
namespace linux {

inline static const char* GetDllError() {
  char* err = dlerror();
  if (err) {
    return err;
  } else {
    return "No error";
  }
}

DllHandle InternalLoadDll(absl::string_view dll_name) {
  DllHandle handle = dlopen(std::string(dll_name).c_str(), RTLD_NOW);
  if (handle == kInvalidDllHandle) {
    AVE_LOG(LS_WARNING) << "Can't load " << dll_name << " : " << GetDllError();
  }
  return handle;
}

void InternalUnloadDll(DllHandle handle) {
  if (dlclose(handle) != 0) {
    AVE_LOG(LS_ERROR) << "Failed to unload library: " << GetDllError();
  }
}

static bool LoadSymbol(DllHandle handle,
                       absl::string_view symbol_name,
                       void** symbol) {
  *symbol = dlsym(handle, std::string(symbol_name).c_str());
  char* err = dlerror();
  if (err) {
    AVE_LOG(LS_ERROR) << "Error loading symbol " << symbol_name << " : " << err;
    return false;
  } else if (!*symbol) {
    AVE_LOG(LS_ERROR) << "Symbol " << symbol_name << " is NULL";
    return false;
  }
  return true;
}

bool InternalLoadSymbols(DllHandle handle,
                         int num_symbols,
                         const char* const symbol_names[],
                         void* symbols[]) {
  // Clear any old errors.
  dlerror();

  for (int i = 0; i < num_symbols; ++i) {
    if (!LoadSymbol(handle, symbol_names[i], &symbols[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace linux
}  // namespace media
}  // namespace ave