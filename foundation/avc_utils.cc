/*
 * avc_utils.cc
 * Copyright (C) 2022 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "avc_utils.h"

#include <cstdint>

#include "base/checks.h"
#include "base/errors.h"
#include "base/logging.h"

#include "bit_reader.h"

namespace ave {
namespace media {

unsigned parseUE(BitReader* br) {
  unsigned numZeroes = 0;
  while (br->getBits(1) == 0) {
    ++numZeroes;
  }

  unsigned x = br->getBits(numZeroes);

  return x + (1u << numZeroes) - 1;
}

unsigned parseUEWithFallback(BitReader* br, unsigned fallback) {
  unsigned numZeroes = 0;
  while (br->getBitsWithFallback(1, static_cast<uint32_t>(1)) == 0) {
    ++numZeroes;
  }
  auto x = static_cast<uint32_t>(0);
  if (numZeroes < 32) {
    if (br->getBitsGraceful(numZeroes, &x)) {
      return x + (1u << numZeroes) - 1;
    }
    return fallback;
  }

  br->skipBits(numZeroes);
  return fallback;
}

signed parseSE(BitReader* br) {
  unsigned codeNum = parseUE(br);

  return (codeNum & 1) ? (codeNum + 1) / 2 : -signed(codeNum / 2);
}

signed parseSEWithFallback(BitReader* br, signed fallback) {
  // NOTE: parseUE cannot normally return ~0 as the max supported value is
  // 0xFFFE
  unsigned codeNum = parseUEWithFallback(br, ~0U);
  if (codeNum == ~0U) {
    return fallback;
  }
  return (codeNum & 1) ? (codeNum + 1) / 2 : -signed(codeNum / 2);
}

static void skipScalingList(BitReader* br, size_t sizeOfScalingList) {
  size_t lastScale = 8;
  size_t nextScale = 8;
  for (size_t j = 0; j < sizeOfScalingList; ++j) {
    if (nextScale != 0) {
      signed delta_scale = parseSE(br);
      // ISO_IEC_14496-10_201402-ITU, 7.4.2.1.1.1, The value of delta_scale
      // shall be in the range of −128 to +127, inclusive.
      if (delta_scale < -128) {
        AVE_LOG(LS_WARNING) << "delta_scale (" << delta_scale
                            << ") is below range, capped to -128";
        delta_scale = -128;
      } else if (delta_scale > 127) {
        AVE_LOG(LS_WARNING) << "delta_scale (" << delta_scale
                            << ") is above range, capped to 127";
        delta_scale = 127;
      }
      nextScale = (lastScale + (delta_scale + 256)) % 256;
    }

    lastScale = (nextScale == 0) ? lastScale : nextScale;
  }
}

// Determine video dimensions from the sequence parameterset.
void FindAVCDimensions(const std::shared_ptr<Buffer>& seqParamSet,
                       int32_t* width,
                       int32_t* height,
                       int32_t* sarWidth,
                       int32_t* sarHeight) {
  BitReader br(seqParamSet->data() + 1, seqParamSet->size() - 1);

  unsigned profile_idc = br.getBits(8);
  br.skipBits(16);
  parseUE(&br);  // seq_parameter_set_id

  unsigned chroma_format_idc = 1;  // 4:2:0 chroma format

  if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
      profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
      profile_idc == 86) {
    chroma_format_idc = parseUE(&br);
    if (chroma_format_idc == 3) {
      br.skipBits(1);  // residual_colour_transform_flag
    }
    parseUE(&br);    // bit_depth_luma_minus8
    parseUE(&br);    // bit_depth_chroma_minus8
    br.skipBits(1);  // qpprime_y_zero_transform_bypass_flag

    if (br.getBits(1)) {  // seq_scaling_matrix_present_flag
      for (size_t i = 0; i < 8; ++i) {
        if (br.getBits(1)) {  // seq_scaling_list_present_flag[i]

          // WARNING: the code below has not ever been exercised...
          // need a real-world example.

          if (i < 6) {
            // ScalingList4x4[i],16,...
            skipScalingList(&br, 16);
          } else {
            // ScalingList8x8[i-6],64,...
            skipScalingList(&br, 64);
          }
        }
      }
    }
  }

  parseUE(&br);  // log2_max_frame_num_minus4
  unsigned pic_order_cnt_type = parseUE(&br);

  if (pic_order_cnt_type == 0) {
    parseUE(&br);  // log2_max_pic_order_cnt_lsb_minus4
  } else if (pic_order_cnt_type == 1) {
    // offset_for_non_ref_pic, offset_for_top_to_bottom_field and
    // offset_for_ref_frame are technically se(v), but since we are
    // just skipping over them the midpoint does not matter.

    br.getBits(1);  // delta_pic_order_always_zero_flag
    parseUE(&br);   // offset_for_non_ref_pic
    parseUE(&br);   // offset_for_top_to_bottom_field

    unsigned num_ref_frames_in_pic_order_cnt_cycle = parseUE(&br);
    for (unsigned i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
      parseUE(&br);  // offset_for_ref_frame
    }
  }

  parseUE(&br);   // num_ref_frames
  br.getBits(1);  // gaps_in_frame_num_value_allowed_flag

  unsigned pic_width_in_mbs_minus1 = parseUE(&br);
  unsigned pic_height_in_map_units_minus1 = parseUE(&br);
  unsigned frame_mbs_only_flag = br.getBits(1);

  //    *width = pic_width_in_mbs_minus1 * 16 + 16;
  if (__builtin_mul_overflow(pic_width_in_mbs_minus1, 16,
                             &pic_width_in_mbs_minus1) ||
      __builtin_add_overflow(pic_width_in_mbs_minus1, 16, width)) {
    *width = 0;
  }

  //    *height = (2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 *
  //    16 + 16);
  if (__builtin_mul_overflow(pic_height_in_map_units_minus1, 16,
                             &pic_height_in_map_units_minus1) ||
      __builtin_add_overflow(pic_height_in_map_units_minus1, 16,
                             &pic_height_in_map_units_minus1) ||
      __builtin_mul_overflow(pic_height_in_map_units_minus1,
                             (2 - frame_mbs_only_flag), height)) {
    *height = 0;
  }

  if (!frame_mbs_only_flag) {
    br.getBits(1);  // mb_adaptive_frame_field_flag
  }

  br.getBits(1);  // direct_8x8_inference_flag

  if (br.getBits(1)) {  // frame_cropping_flag
    unsigned frame_crop_left_offset = parseUE(&br);
    unsigned frame_crop_right_offset = parseUE(&br);
    unsigned frame_crop_top_offset = parseUE(&br);
    unsigned frame_crop_bottom_offset = parseUE(&br);

    unsigned cropUnitX = 0, cropUnitY = 0;
    if (chroma_format_idc == 0 /* monochrome */) {
      cropUnitX = 1;
      cropUnitY = 2 - frame_mbs_only_flag;
    } else {
      unsigned subWidthC = (chroma_format_idc == 3) ? 1 : 2;
      unsigned subHeightC = (chroma_format_idc == 1) ? 2 : 1;

      cropUnitX = subWidthC;
      cropUnitY = subHeightC * (2 - frame_mbs_only_flag);
    }

    AVE_LOG(LS_VERBOSE) << "frame_crop = (" << frame_crop_left_offset << ", "
                        << frame_crop_right_offset << ", "
                        << frame_crop_top_offset << ", "
                        << frame_crop_bottom_offset
                        << "), cropUnitX = " << cropUnitX
                        << ", cropUnitY = " << cropUnitY;

    // *width -= (frame_crop_left_offset + frame_crop_right_offset) *
    // cropUnitX;
    if (__builtin_add_overflow(frame_crop_left_offset, frame_crop_right_offset,
                               &frame_crop_left_offset) ||
        __builtin_mul_overflow(frame_crop_left_offset, cropUnitX,
                               &frame_crop_left_offset) ||
        __builtin_sub_overflow(*width, frame_crop_left_offset, width) ||
        *width < 0) {
      *width = 0;
    }

    //*height -= (frame_crop_top_offset + frame_crop_bottom_offset) *
    // cropUnitY;
    if (__builtin_add_overflow(frame_crop_top_offset, frame_crop_bottom_offset,
                               &frame_crop_top_offset) ||
        __builtin_mul_overflow(frame_crop_top_offset, cropUnitY,
                               &frame_crop_top_offset) ||
        __builtin_sub_overflow(*height, frame_crop_top_offset, height) ||
        *height < 0) {
      *height = 0;
    }
  }

  if (sarWidth != nullptr) {
    *sarWidth = 0;
  }

  if (sarHeight != nullptr) {
    *sarHeight = 0;
  }

  if (br.getBits(1)) {  // vui_parameters_present_flag
    unsigned sar_width = 0, sar_height = 0;

    if (br.getBits(1)) {  // aspect_ratio_info_present_flag
      unsigned aspect_ratio_idc = br.getBits(8);

      if (aspect_ratio_idc == 255 /* extendedSAR */) {
        sar_width = br.getBits(16);
        sar_height = br.getBits(16);
      } else {
        // NOLINTBEGIN(modernize-avoid-c-arrays)
        static const struct {
          unsigned width, height;
        } kFixedSARs[] = {
            {0, 0},  // Invalid
            {1, 1},    {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11},
            {20, 11},  {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33},
            {160, 99}, {4, 3},   {3, 2},   {2, 1},
        };
        // NOLINTEND(modernize-avoid-c-arrays)

        if (aspect_ratio_idc > 0 && aspect_ratio_idc < NELEM(kFixedSARs)) {
          sar_width = kFixedSARs[aspect_ratio_idc].width;
          sar_height = kFixedSARs[aspect_ratio_idc].height;
        }
      }
    }

    AVE_LOG(LS_VERBOSE) << "sample aspect ratio = " << sar_width << " : "
                        << sar_height;

    if (sarWidth != nullptr) {
      *sarWidth = static_cast<int32_t>(sar_width);
    }

    if (sarHeight != nullptr) {
      *sarHeight = static_cast<int32_t>(sar_height);
    }
  }
}

status_t getNextNALUnit(const uint8_t** _data,
                        size_t* _size,
                        const uint8_t** nalStart,
                        size_t* nalSize,
                        bool startCodeFollows) {
  const uint8_t* data = *_data;
  size_t size = *_size;

  *nalStart = nullptr;
  *nalSize = 0;

  if (size < 3) {
    return ave::E_AGAIN;
  }

  size_t offset = 0;

  // A valid startcode consists of at least two 0x00 bytes followed by 0x01.
  for (; offset + 2 < size; ++offset) {
    if (data[offset + 2] == 0x01 && data[offset] == 0x00 &&
        data[offset + 1] == 0x00) {
      break;
    }
  }
  if (offset + 2 >= size) {
    *_data = &data[offset];
    *_size = 2;
    return ave::E_AGAIN;
  }
  offset += 3;

  size_t startOffset = offset;

  for (;;) {
    while (offset < size && data[offset] != 0x01) {
      ++offset;
    }

    if (offset == size) {
      if (startCodeFollows) {
        offset = size + 2;
        break;
      }

      return ave::E_AGAIN;
    }

    if (data[offset - 1] == 0x00 && data[offset - 2] == 0x00) {
      break;
    }

    ++offset;
  }

  size_t endOffset = offset - 2;
  while (endOffset > startOffset + 1 && data[endOffset - 1] == 0x00) {
    --endOffset;
  }

  *nalStart = &data[startOffset];
  *nalSize = endOffset - startOffset;

  if (offset + 2 < size) {
    *_data = &data[offset - 2];
    *_size = size - offset + 2;
  } else {
    *_data = nullptr;
    *_size = 0;
  }

  return OK;
}

static std::shared_ptr<Buffer> FindNAL(const uint8_t* data,
                                       size_t size,
                                       unsigned nalType) {
  const uint8_t* nalStart = nullptr;
  size_t nalSize = 0;
  while (getNextNALUnit(&data, &size, &nalStart, &nalSize, true) == OK) {
    if (nalSize > 0 && (nalStart[0] & 0x1f) == nalType) {
      auto buffer = std::make_shared<Buffer>(nalSize);
      memcpy(buffer->data(), nalStart, nalSize);
      return buffer;
    }
  }

  return nullptr;
}

const char* AVCProfileToString(uint8_t profile) {
  switch (profile) {
    case kAVCProfileBaseline:
      return "Baseline";
    case kAVCProfileMain:
      return "Main";
    case kAVCProfileExtended:
      return "Extended";
    case kAVCProfileHigh:
      return "High";
    case kAVCProfileHigh10:
      return "High 10";
    case kAVCProfileHigh422:
      return "High 422";
    case kAVCProfileHigh444:
      return "High 444";
    case kAVCProfileCAVLC444Intra:
      return "CAVLC 444 Intra";
    default:
      return "Unknown";
  }
}

std::shared_ptr<Buffer> MakeAVCCodecSpecificData(
    const std::shared_ptr<Buffer>& accessUnit,
    int32_t* width,
    int32_t* height,
    int32_t* sarWidth,
    int32_t* sarHeight) {
  const uint8_t* data = accessUnit->data();
  size_t size = accessUnit->size();

  std::shared_ptr<Buffer> seqParamSet = FindNAL(data, size, 7);
  if (seqParamSet == nullptr) {
    return nullptr;
  }

  FindAVCDimensions(seqParamSet, width, height, sarWidth, sarHeight);

  std::shared_ptr<Buffer> picParamSet = FindNAL(data, size, 8);
  AVE_CHECK(picParamSet != nullptr);

  size_t csdSize = 1 + 3 + 1 + 1 + 2 * 1 + seqParamSet->size() + 1 + 2 * 1 +
                   picParamSet->size();

  auto csd = std::make_shared<Buffer>(csdSize);
  uint8_t* out = csd->data();

  *out++ = 0x01;                            // configurationVersion
  memcpy(out, seqParamSet->data() + 1, 3);  // profile/level...

  uint8_t profile = out[0];
  uint8_t level = out[2];

  out += 3;
  *out++ = (0x3f << 2) | 1;  // lengthSize == 2 bytes
  *out++ = 0xe0 | 1;

  *out++ = seqParamSet->size() >> 8;
  *out++ = seqParamSet->size() & 0xff;
  memcpy(out, seqParamSet->data(), seqParamSet->size());
  out += seqParamSet->size();

  *out++ = 1;

  *out++ = picParamSet->size() >> 8;
  *out++ = picParamSet->size() & 0xff;
  memcpy(out, picParamSet->data(), picParamSet->size());

#if 0
    AVE_LOG(LS_INFO) << "AVC seq param set");
    hexdump(seqParamSet->data(), seqParamSet->size());
#endif

  if (sarWidth != nullptr && sarHeight != nullptr) {
    if ((*sarWidth > 0 && *sarHeight > 0) &&
        (*sarWidth != 1 || *sarHeight != 1)) {
      AVE_LOG(LS_INFO) << "found AVC codec config (" << *width << " x "
                       << *height << ", " << AVCProfileToString(profile)
                       << "-profile level " << level / 10 << "." << level % 10
                       << ") SAR " << *sarWidth << " : " << *sarHeight;
    } else {
      // We treat *:0 and 0:* (unspecified) as 1:1.
      *sarWidth = 0;
      *sarHeight = 0;
      AVE_LOG(LS_INFO) << "found AVC codec config (" << *width << " x "
                       << *height << ", " << AVCProfileToString(profile)
                       << "-profile level " << level / 10 << "." << level % 10
                       << ")";
    }
  }
  return csd;
}

bool IsIDR(const uint8_t* data, size_t size) {
  //    const uint8_t *data = buffer->data();
  //    size_t size = buffer->size();
  bool foundIDR = false;

  const uint8_t* nalStart = nullptr;
  size_t nalSize = 0;
  while (getNextNALUnit(&data, &size, &nalStart, &nalSize, true) == OK) {
    if (nalSize == 0u) {
      AVE_LOG(LS_WARNING)
          << "skipping empty nal unit from potentially malformed bitstream";
      continue;
    }

    unsigned nalType = nalStart[0] & 0x1f;

    if (nalType == 5) {
      foundIDR = true;
      break;
    }
  }

  return foundIDR;
}

bool IsAVCReferenceFrame(const std::shared_ptr<Buffer>& accessUnit) {
  const uint8_t* data = accessUnit->data();
  size_t size = accessUnit->size();
  if (data == nullptr) {
    AVE_LOG(LS_ERROR) << "IsAVCReferenceFrame: called on NULL data ("
                      << accessUnit.get() << ", " << size << ")";
    return false;
  }

  const uint8_t* nalStart = nullptr;
  size_t nalSize = 0;
  while (getNextNALUnit(&data, &size, &nalStart, &nalSize, true) == OK) {
    if (nalSize == 0) {
      AVE_LOG(LS_ERROR) << "IsAVCReferenceFrame: invalid nalSize: 0 ("
                        << accessUnit.get() << ", " << size << ")";
      return false;
    }

    unsigned nalType = nalStart[0] & 0x1f;

    if (nalType == 5) {
      return true;
    }
    unsigned nal_ref_idc = (nalStart[0] >> 5) & 3;
    return nal_ref_idc != 0;
  }

  return true;
}

uint32_t FindAVCLayerId(const uint8_t* data, size_t size) {
  AVE_CHECK(data != nullptr);

  const unsigned kSvcNalType = 0xE;
  const unsigned kSvcNalSearchRange = 32;
  // SVC NAL
  // |---0 1110|1--- ----|---- ----|iii- ---|
  //       ^                        ^
  //   NAL-type = 0xE               layer-Id
  //
  // layer_id 0 is for base layer, while 1, 2, ... are enhancement layers.
  // Layer n uses reference frames from layer 0, 1, ..., n-1.

  auto layerId = static_cast<uint32_t>(0);
  std::shared_ptr<Buffer> svcNAL = FindNAL(
      data, size > kSvcNalSearchRange ? kSvcNalSearchRange : size, kSvcNalType);
  if (svcNAL != nullptr && svcNAL->size() >= 4) {
    layerId = static_cast<uint32_t>((*(svcNAL->data() + 3) >> 5) & 0x7);
  }
  return layerId;
}

bool ExtractDimensionsFromVOLHeader(const uint8_t* data,
                                    size_t size,
                                    int32_t* width,
                                    int32_t* height) {
  BitReader br(&data[4], size - 4);
  br.skipBits(1);  // random_accessible_vol
  unsigned video_object_type_indication = br.getBits(8);

  AVE_CHECK_NE(video_object_type_indication,
               0x21u /* Fine Granularity Scalable */);

  if (br.getBits(1)) {
    br.skipBits(4);  // video_object_layer_verid
    br.skipBits(3);  // video_object_layer_priority
  }
  unsigned aspect_ratio_info = br.getBits(4);
  if (aspect_ratio_info == 0x0f /* extended PAR */) {
    br.skipBits(8);  // par_width
    br.skipBits(8);  // par_height
  }
  if (br.getBits(1)) {           // vol_control_parameters
    br.skipBits(2);              // chroma_format
    br.skipBits(1);              // low_delay
    if (br.getBits(1)) {         // vbv_parameters
      br.skipBits(15);           // first_half_bit_rate
      AVE_CHECK(br.getBits(1));  // marker_bit
      br.skipBits(15);           // latter_half_bit_rate
      AVE_CHECK(br.getBits(1));  // marker_bit
      br.skipBits(15);           // first_half_vbv_buffer_size
      AVE_CHECK(br.getBits(1));  // marker_bit
      br.skipBits(3);            // latter_half_vbv_buffer_size
      br.skipBits(11);           // first_half_vbv_occupancy
      AVE_CHECK(br.getBits(1));  // marker_bit
      br.skipBits(15);           // latter_half_vbv_occupancy
      AVE_CHECK(br.getBits(1));  // marker_bit
    }
  }
  unsigned video_object_layer_shape = br.getBits(2);
  AVE_CHECK_EQ(video_object_layer_shape, 0x00u /* rectangular */);

  AVE_CHECK(br.getBits(1));  // marker_bit
  unsigned vop_time_increment_resolution = br.getBits(16);
  AVE_CHECK(br.getBits(1));  // marker_bit

  if (br.getBits(1)) {  // fixed_vop_rate
    // range [0..vop_time_increment_resolution)

    // vop_time_increment_resolution
    // 2 => 0..1, 1 bit
    // 3 => 0..2, 2 bits
    // 4 => 0..3, 2 bits
    // 5 => 0..4, 3 bits
    // ...

    AVE_CHECK_GT(vop_time_increment_resolution, 0u);
    --vop_time_increment_resolution;

    unsigned numBits = 0;
    while (vop_time_increment_resolution > 0) {
      ++numBits;
      vop_time_increment_resolution >>= 1;
    }

    br.skipBits(numBits);  // fixed_vop_time_increment
  }

  AVE_CHECK(br.getBits(1));  // marker_bit
  unsigned video_object_layer_width = br.getBits(13);
  AVE_CHECK(br.getBits(1));  // marker_bit
  unsigned video_object_layer_height = br.getBits(13);
  AVE_CHECK(br.getBits(1));  // marker_bit

  br.skipBits(1);  // interlaced

  *width = static_cast<uint32_t>(video_object_layer_width);
  *height = static_cast<uint32_t>(video_object_layer_height);

  return true;
}

bool GetMPEGAudioFrameSize(uint32_t header,
                           size_t* frame_size,
                           int* out_sampling_rate,
                           int* out_channels,
                           int* out_bitrate,
                           int* out_num_samples) {
  *frame_size = 0;

  if (out_sampling_rate) {
    *out_sampling_rate = 0;
  }

  if (out_channels) {
    *out_channels = 0;
  }

  if (out_bitrate) {
    *out_bitrate = 0;
  }

  if (out_num_samples) {
    *out_num_samples = 1152;
  }

  if ((header & 0xffe00000) != 0xffe00000) {
    return false;
  }

  unsigned version = (header >> 19) & 3;

  if (version == 0x01) {
    return false;
  }

  unsigned layer = (header >> 17) & 3;

  if (layer == 0x00) {
    return false;
  }

  // we can get protection value from (header >> 16) & 1

  unsigned bitrate_index = (header >> 12) & 0x0f;

  if (bitrate_index == 0 || bitrate_index == 0x0f) {
    // Disallow "free" bitrate.
    return false;
  }

  unsigned sampling_rate_index = (header >> 10) & 3;

  if (sampling_rate_index == 3) {
    return false;
  }

  // NOLINTBEGIN(modernize-avoid-c-arrays)
  static const int kSamplingRateV1[] = {44100, 48000, 32000};
  // NOLINTEND(modernize-avoid-c-arrays)
  int sampling_rate = kSamplingRateV1[sampling_rate_index];
  if (version == 2 /* V2 */) {
    sampling_rate /= 2;
  } else if (version == 0 /* V2.5 */) {
    sampling_rate /= 4;
  }

  unsigned padding = (header >> 9) & 1;

  if (layer == 3) {
    // layer I
    // NOLINTBEGIN(modernize-avoid-c-arrays)
    static const int kBitrateV1[] = {32,  64,  96,  128, 160, 192, 224,
                                     256, 288, 320, 352, 384, 416, 448};
    static const int kBitrateV2[] = {32,  48,  56,  64,  80,  96,  112,
                                     128, 144, 160, 176, 192, 224, 256};
    // NOLINTEND(modernize-avoid-c-arrays)

    int bitrate = (version == 3 /* V1 */) ? kBitrateV1[bitrate_index - 1]
                                          : kBitrateV2[bitrate_index - 1];

    if (out_bitrate) {
      *out_bitrate = bitrate;
    }

    *frame_size = (12000 * bitrate / sampling_rate + padding) * 4;

    if (out_num_samples) {
      *out_num_samples = 384;
    }
  } else {
    // layer II or III
    // NOLINTBEGIN(modernize-avoid-c-arrays)
    static const int kBitrateV1L2[] = {32,  48,  56,  64,  80,  96,  112,
                                       128, 160, 192, 224, 256, 320, 384};

    static const int kBitrateV1L3[] = {32,  40,  48,  56,  64,  80,  96,
                                       112, 128, 160, 192, 224, 256, 320};

    static const int kBitrateV2[] = {8,  16, 24, 32,  40,  48,  56,
                                     64, 80, 96, 112, 128, 144, 160};
    // NOLINTEND(modernize-avoid-c-arrays)

    int bitrate = 0;
    if (version == 3 /* V1 */) {
      bitrate = (layer == 2 /* L2 */) ? kBitrateV1L2[bitrate_index - 1]
                                      : kBitrateV1L3[bitrate_index - 1];

      if (out_num_samples) {
        *out_num_samples = 1152;
      }
    } else {
      // V2 (or 2.5)

      bitrate = kBitrateV2[bitrate_index - 1];
      if (out_num_samples) {
        *out_num_samples = (layer == 1 /* L3 */) ? 576 : 1152;
      }
    }

    if (out_bitrate) {
      *out_bitrate = bitrate;
    }

    if (version == 3 /* V1 */) {
      *frame_size = 144000 * bitrate / sampling_rate + padding;
    } else {
      // V2 or V2.5
      size_t tmp = (layer == 1 /* L3 */) ? 72000 : 144000;
      *frame_size = tmp * bitrate / sampling_rate + padding;
    }
  }

  if (out_sampling_rate) {
    *out_sampling_rate = sampling_rate;
  }

  if (out_channels) {
    int channel_mode = (header >> 6) & 3;
    *out_channels = (channel_mode == 3) ? 1 : 2;
  }

  return true;
}
}  // namespace media
}  // namespace ave
