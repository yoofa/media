/*
 * module_fec_types.h - FEC type definitions
 *
 * Copyright (c) 2025 The AveStart project authors. All Rights Reserved.
 *
 * Ported from WebRTC project.
 */

#ifndef MEDIA_MODULES_INCLUDE_MODULE_FEC_TYPES_H_
#define MEDIA_MODULES_INCLUDE_MODULE_FEC_TYPES_H_

namespace ave {

// Types for the FEC packet masks. The type `kFecMaskRandom` is based on a
// random loss model. The type `kFecMaskBursty` is based on a bursty/consecutive
// loss model.
enum FecMaskType {
  kFecMaskRandom,
  kFecMaskBursty,
};

// Struct containing forward error correction settings.
struct FecProtectionParams {
  int fec_rate = 0;
  int max_fec_frames = 0;
  FecMaskType fec_mask_type = FecMaskType::kFecMaskRandom;
};

}  // namespace ave

#endif  // MEDIA_MODULES_INCLUDE_MODULE_FEC_TYPES_H_
