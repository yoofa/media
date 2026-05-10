/*
 * color_space.h - Color space information
 *
 * Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 * Ported to aspect-view-engine (AVE) project.
 */

#ifndef MEDIA_FOUNDATION_COLOR_SPACE_H_
#define MEDIA_FOUNDATION_COLOR_SPACE_H_

#include <cstdint>
#include <optional>

#include "media/foundation/hdr_metadata.h"

namespace ave {
namespace media {

// This class represents color information as specified in T-REC H.273,
// available from https://www.itu.int/rec/T-REC-H.273.
class ColorSpace {
 public:
  enum class PrimaryID : uint8_t {
    // The indices are equal to the values specified in T-REC H.273 Table 2.
    kBT709 = 1,
    kUnspecified = 2,
    kBT470M = 4,
    kBT470BG = 5,
    kSMPTE170M = 6,  // Identical to BT601
    kSMPTE240M = 7,
    kFILM = 8,
    kBT2020 = 9,
    kSMPTEST428 = 10,
    kSMPTEST431 = 11,
    kSMPTEST432 = 12,
    kJEDECP22 = 22,  // Identical to EBU3213-E
  };

  enum class TransferID : uint8_t {
    // The indices are equal to the values specified in T-REC H.273 Table 3.
    kBT709 = 1,
    kUnspecified = 2,
    kGAMMA22 = 4,
    kGAMMA28 = 5,
    kSMPTE170M = 6,
    kSMPTE240M = 7,
    kLINEAR = 8,
    kLOG = 9,
    kLOG_SQRT = 10,
    kIEC61966_2_4 = 11,
    kBT1361_ECG = 12,
    kIEC61966_2_1 = 13,
    kBT2020_10 = 14,
    kBT2020_12 = 15,
    kSMPTEST2084 = 16,
    kSMPTEST428 = 17,
    kARIB_STD_B67 = 18,
  };

  enum class MatrixID : uint8_t {
    // The indices are equal to the values specified in T-REC H.273 Table 4.
    kRGB = 0,
    kBT709 = 1,
    kUnspecified = 2,
    kFCC = 4,
    kBT470BG = 5,
    kSMPTE170M = 6,
    kSMPTE240M = 7,
    kYCOCG = 8,
    kBT2020_NCL = 9,
    kBT2020_CL = 10,
    kSMPTE2085 = 11,
    kCDNCLS = 12,
    kCDCLS = 13,
    kBT2100_ICTCP = 14,
  };

  enum class RangeID {
    kInvalid = 0,
    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    kLimited = 1,
    // Full RGB color range with RGB values from 0 to 255.
    kFull = 2,
    // Range is defined by MatrixCoefficients/TransferCharacteristics.
    kDerived = 3,
  };

  enum class ChromaSiting {
    kUnspecified = 0,
    kCollocated = 1,
    kHalf = 2,
  };

  ColorSpace() = default;
  ColorSpace(const ColorSpace& other) = default;
  ColorSpace(ColorSpace&& other) = default;
  ColorSpace& operator=(const ColorSpace& other) = default;

  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID range)
      : primaries_(primaries),
        transfer_(transfer),
        matrix_(matrix),
        range_(range) {}

  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID range,
             ChromaSiting chroma_siting_horizontal,
             ChromaSiting chroma_siting_vertical,
             const HdrMetadata* hdr_metadata)
      : primaries_(primaries),
        transfer_(transfer),
        matrix_(matrix),
        range_(range),
        chroma_siting_horizontal_(chroma_siting_horizontal),
        chroma_siting_vertical_(chroma_siting_vertical),
        hdr_metadata_(hdr_metadata ? ::std::make_optional(*hdr_metadata)
                                   : ::std::nullopt) {}

  friend bool operator==(const ColorSpace& lhs, const ColorSpace& rhs) {
    return lhs.primaries_ == rhs.primaries_ && lhs.transfer_ == rhs.transfer_ &&
           lhs.matrix_ == rhs.matrix_ && lhs.range_ == rhs.range_ &&
           lhs.chroma_siting_horizontal_ == rhs.chroma_siting_horizontal_ &&
           lhs.chroma_siting_vertical_ == rhs.chroma_siting_vertical_ &&
           lhs.hdr_metadata_ == rhs.hdr_metadata_;
  }
  friend bool operator!=(const ColorSpace& lhs, const ColorSpace& rhs) {
    return !(lhs == rhs);
  }

  PrimaryID primaries() const { return primaries_; }
  TransferID transfer() const { return transfer_; }
  MatrixID matrix() const { return matrix_; }
  RangeID range() const { return range_; }
  ChromaSiting chroma_siting_horizontal() const {
    return chroma_siting_horizontal_;
  }
  ChromaSiting chroma_siting_vertical() const {
    return chroma_siting_vertical_;
  }
  const HdrMetadata* hdr_metadata() const {
    return hdr_metadata_.has_value() ? &hdr_metadata_.value() : nullptr;
  }

  bool set_primaries_from_uint8(uint8_t enum_value) {
    if (enum_value > 22) {
      return false;
    }
    primaries_ = static_cast<PrimaryID>(enum_value);
    return true;
  }

  bool set_transfer_from_uint8(uint8_t enum_value) {
    if (enum_value > 18) {
      return false;
    }
    transfer_ = static_cast<TransferID>(enum_value);
    return true;
  }

  bool set_matrix_from_uint8(uint8_t enum_value) {
    if (enum_value > 14) {
      return false;
    }
    matrix_ = static_cast<MatrixID>(enum_value);
    return true;
  }

  bool set_range_from_uint8(uint8_t enum_value) {
    if (enum_value > 3) {
      return false;
    }
    range_ = static_cast<RangeID>(enum_value);
    return true;
  }

  bool set_chroma_siting_horizontal_from_uint8(uint8_t enum_value) {
    if (enum_value > 2) {
      return false;
    }
    chroma_siting_horizontal_ = static_cast<ChromaSiting>(enum_value);
    return true;
  }

  bool set_chroma_siting_vertical_from_uint8(uint8_t enum_value) {
    if (enum_value > 2) {
      return false;
    }
    chroma_siting_vertical_ = static_cast<ChromaSiting>(enum_value);
    return true;
  }

  void set_hdr_metadata(const HdrMetadata* hdr_metadata) {
    hdr_metadata_ =
        hdr_metadata ? ::std::make_optional(*hdr_metadata) : ::std::nullopt;
  }

 private:
  PrimaryID primaries_ = PrimaryID::kUnspecified;
  TransferID transfer_ = TransferID::kUnspecified;
  MatrixID matrix_ = MatrixID::kUnspecified;
  RangeID range_ = RangeID::kInvalid;
  ChromaSiting chroma_siting_horizontal_ = ChromaSiting::kUnspecified;
  ChromaSiting chroma_siting_vertical_ = ChromaSiting::kUnspecified;
  ::std::optional<HdrMetadata> hdr_metadata_;
};

}  // namespace media
}  // namespace ave

#endif  // MEDIA_FOUNDATION_COLOR_SPACE_H_
