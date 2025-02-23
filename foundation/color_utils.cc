/*
 * color_utils.cc
 * Copyright (C) 2022 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "color_utils.h"

#include "base/checks.h"
#include "base/logging.h"

#include "Lookup.h"
#include "buffer.h"

namespace ave {
namespace media {

// shortcut names for brevity in the following tables
using CA = ColorAspects;
using CU = ColorUtils;

#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)

const static Lookup<CU::ColorRange, CA::Range> sRanges{{
    {CU::kColorRangeLimited, CA::RangeLimited},
    {CU::kColorRangeFull, CA::RangeFull},
    {CU::kColorRangeUnspecified, CA::RangeUnspecified},
}};

const static Lookup<CU::ColorStandard,
                    std::pair<CA::Primaries, CA::MatrixCoeffs>>
    sStandards{{
        {CU::kColorStandardUnspecified,
         {CA::PrimariesUnspecified, CA::MatrixUnspecified}},
        {CU::kColorStandardBT709, {CA::PrimariesBT709_5, CA::MatrixBT709_5}},
        {CU::kColorStandardBT601_625,
         {CA::PrimariesBT601_6_625, CA::MatrixBT601_6}},
        {CU::kColorStandardBT601_625_Unadjusted,
         // this is a really close match
         {CA::PrimariesBT601_6_625, CA::MatrixBT709_5}},
        {CU::kColorStandardBT601_525,
         {CA::PrimariesBT601_6_525, CA::MatrixBT601_6}},
        {CU::kColorStandardBT601_525_Unadjusted,
         {CA::PrimariesBT601_6_525, CA::MatrixSMPTE240M}},
        {CU::kColorStandardBT2020, {CA::PrimariesBT2020, CA::MatrixBT2020}},
        {CU::kColorStandardBT2020Constant,
         {CA::PrimariesBT2020, CA::MatrixBT2020Constant}},
        {CU::kColorStandardBT470M, {CA::PrimariesBT470_6M, CA::MatrixBT470_6M}},
        // NOTE: there is no close match to the matrix used by standard film,
        // chose closest
        {CU::kColorStandardFilm, {CA::PrimariesGenericFilm, CA::MatrixBT2020}},
    }};

const static Lookup<CU::ColorTransfer, CA::Transfer> sTransfers{{
    {CU::kColorTransferUnspecified, CA::TransferUnspecified},
    {CU::kColorTransferLinear, CA::TransferLinear},
    {CU::kColorTransferSRGB, CA::TransferSRGB},
    {CU::kColorTransferSMPTE_170M, CA::TransferSMPTE170M},
    {CU::kColorTransferGamma22, CA::TransferGamma22},
    {CU::kColorTransferGamma28, CA::TransferGamma28},
    {CU::kColorTransferST2084, CA::TransferST2084},
    {CU::kColorTransferHLG, CA::TransferHLG},
}};

static bool isValid(ColorAspects::Primaries p) {
  return p <= ColorAspects::PrimariesOther;
}

static bool isDefined(ColorAspects::Primaries p) {
  return p <= ColorAspects::PrimariesBT2020;
}

static bool isValid(ColorAspects::MatrixCoeffs c) {
  return c <= ColorAspects::MatrixOther;
}

static bool isDefined(ColorAspects::MatrixCoeffs c) {
  return c <= ColorAspects::MatrixBT2020Constant;
}

// static
int32_t ColorUtils::wrapColorAspectsIntoColorStandard(
    ColorAspects::Primaries primaries,
    ColorAspects::MatrixCoeffs coeffs) {
  ColorStandard res = kColorStandardUnspecified;

  if (sStandards.map(std::make_pair(primaries, coeffs), &res)) {
    return static_cast<int32_t>(res);
  }

  if (!isValid(primaries) || !isValid(coeffs)) {
    return static_cast<int32_t>(kColorStandardUnspecified);
  }

  // check platform media limits
  uint32_t numPrimaries = ColorAspects::PrimariesBT2020 + 1;
  if (isDefined(primaries) && isDefined(coeffs)) {
    return static_cast<int32_t>(kColorStandardExtendedStart + primaries +
                                coeffs * numPrimaries);
  }
  return static_cast<int32_t>(kColorStandardVendorStart + primaries +
                              coeffs * 0x100);
}

// static
status_t ColorUtils::unwrapColorAspectsFromColorStandard(
    int32_t standard,
    ColorAspects::Primaries* primaries,
    ColorAspects::MatrixCoeffs* coeffs) {
  std::pair<ColorAspects::Primaries, ColorAspects::MatrixCoeffs> res;
  if (sStandards.map(static_cast<ColorStandard>(standard), &res)) {
    *primaries = res.first;
    *coeffs = res.second;
    return OK;
  }

  auto start = static_cast<int32_t>(kColorStandardExtendedStart);
  auto numPrimaries = static_cast<int32_t>(ColorAspects::PrimariesBT2020) + 1;
  auto numCoeffs = static_cast<int32_t>(ColorAspects::MatrixBT2020Constant) + 1;
  if (standard >= static_cast<int32_t>(kColorStandardVendorStart)) {
    start = static_cast<int32_t>(kColorStandardVendorStart);
    numPrimaries =
        static_cast<int32_t>(ColorAspects::PrimariesOther) + 1;       // 0x100
    numCoeffs = static_cast<int32_t>(ColorAspects::MatrixOther) + 1;  // 0x100;
  }
  if (standard >= start && standard < start + numPrimaries * numCoeffs) {
    auto product = static_cast<int32_t>(standard - start);
    *primaries = static_cast<ColorAspects::Primaries>(product % numPrimaries);
    *coeffs = static_cast<ColorAspects::MatrixCoeffs>(product / numPrimaries);
    return OK;
  }
  *primaries = ColorAspects::PrimariesOther;
  *coeffs = ColorAspects::MatrixOther;
  return BAD_VALUE;
}

static bool isValid(ColorAspects::Range r) {
  return r <= ColorAspects::RangeOther;
}

static bool isDefined(ColorAspects::Range r) {
  return r <= ColorAspects::RangeLimited;
}

//  static
int32_t ColorUtils::wrapColorAspectsIntoColorRange(ColorAspects::Range range) {
  ColorRange res = kColorRangeUnspecified;  // Initialize with a default value
  if (sRanges.map(range, &res)) {
    return static_cast<int32_t>(res);
  }
  if (!isValid(range)) {
    return static_cast<int32_t>(kColorRangeUnspecified);
  }
  AVE_CHECK(!isDefined(range));
  // all platform values are in sRanges
  return static_cast<int32_t>(kColorRangeVendorStart + range);
}

// static
status_t ColorUtils::unwrapColorAspectsFromColorRange(
    int32_t range,
    ColorAspects::Range* aspect) {
  if (sRanges.map(static_cast<ColorRange>(range), aspect)) {
    return OK;
  }

  auto start = static_cast<int32_t>(kColorRangeVendorStart);
  auto numRanges = static_cast<int32_t>(ColorAspects::RangeOther) + 1;  // 0x100
  if (range >= start && range < start + numRanges) {
    *aspect = static_cast<ColorAspects::Range>(range - start);
    return OK;
  }
  *aspect = ColorAspects::RangeOther;
  return BAD_VALUE;
}

static bool isValid(ColorAspects::Transfer t) {
  return t <= ColorAspects::TransferOther;
}

static bool isDefined(ColorAspects::Transfer t) {
  return t <= ColorAspects::TransferHLG ||
         (t >= ColorAspects::TransferSMPTE240M &&
          t <= ColorAspects::TransferST428);
}

//  static
int32_t ColorUtils::wrapColorAspectsIntoColorTransfer(
    ColorAspects::Transfer transfer) {
  ColorTransfer res = kColorTransferUnspecified;
  if (sTransfers.map(transfer, &res)) {
    return static_cast<int32_t>(res);
  }
  if (!isValid(transfer)) {
    return static_cast<int32_t>(kColorTransferUnspecified);
  }
  if (isDefined(transfer)) {
    return static_cast<int32_t>(kColorTransferExtendedStart + transfer);
  }
  // all platform values are in sRanges
  return static_cast<int32_t>(kColorTransferVendorStart + transfer);
}

// static
status_t ColorUtils::unwrapColorAspectsFromColorTransfer(
    int32_t transfer,
    ColorAspects::Transfer* aspect) {
  if (sTransfers.map(static_cast<ColorTransfer>(transfer), aspect)) {
    return OK;
  }

  auto start = static_cast<int32_t>(kColorTransferExtendedStart);
  auto numTransfers = static_cast<int32_t>(ColorAspects::TransferST428) + 1;
  if (transfer >= static_cast<int32_t>(kColorTransferVendorStart)) {
    start = static_cast<int32_t>(kColorTransferVendorStart);
    numTransfers = ColorAspects::TransferOther + 1;  // 0x100
  }
  if (transfer >= start && transfer < start + numTransfers) {
    *aspect = static_cast<ColorAspects::Transfer>(transfer - start);
    return OK;
  }
  *aspect = ColorAspects::TransferOther;
  return BAD_VALUE;
}

// static
status_t ColorUtils::convertPlatformColorAspectsToCodecAspects(
    int32_t range,
    int32_t standard,
    int32_t transfer,
    ColorAspects& aspects) {
  status_t res1 = unwrapColorAspectsFromColorRange(range, &aspects.mRange);
  status_t res2 = unwrapColorAspectsFromColorStandard(
      standard, &aspects.mPrimaries, &aspects.mMatrixCoeffs);
  status_t res3 =
      unwrapColorAspectsFromColorTransfer(transfer, &aspects.mTransfer);
  return res1 != OK ? res1 : (res2 != OK ? res2 : res3);
}

// static
status_t ColorUtils::convertCodecColorAspectsToPlatformAspects(
    const ColorAspects& aspects,
    int32_t* range,
    int32_t* standard,
    int32_t* transfer) {
  *range = wrapColorAspectsIntoColorRange(aspects.mRange);
  *standard = wrapColorAspectsIntoColorStandard(aspects.mPrimaries,
                                                aspects.mMatrixCoeffs);
  *transfer = wrapColorAspectsIntoColorTransfer(aspects.mTransfer);
  if (isValid(aspects.mRange) && isValid(aspects.mPrimaries) &&
      isValid(aspects.mMatrixCoeffs) && isValid(aspects.mTransfer)) {
    return OK;
  }
  return BAD_VALUE;
}

const static Lookup<int32_t, ColorAspects::Primaries> sIsoPrimaries{{
    {1, ColorAspects::PrimariesBT709_5},
    {2, ColorAspects::PrimariesUnspecified},
    {4, ColorAspects::PrimariesBT470_6M},
    {5, ColorAspects::PrimariesBT601_6_625},
    {6, ColorAspects::PrimariesBT601_6_525 /* main */},
    {7, ColorAspects::PrimariesBT601_6_525},
    // -- ITU T.832 201201 ends here
    {8, ColorAspects::PrimariesGenericFilm},
    {9, ColorAspects::PrimariesBT2020},
    {10, ColorAspects::PrimariesOther /* XYZ */},
}};

const static Lookup<int32_t, ColorAspects::Transfer> sIsoTransfers{{
    {1, ColorAspects::TransferSMPTE170M /* main */},
    {2, ColorAspects::TransferUnspecified},
    {4, ColorAspects::TransferGamma22},
    {5, ColorAspects::TransferGamma28},
    {6, ColorAspects::TransferSMPTE170M},
    {7, ColorAspects::TransferSMPTE240M},
    {8, ColorAspects::TransferLinear},
    {9, ColorAspects::TransferOther /* log 100:1 */},
    {10, ColorAspects::TransferOther /* log 316:1 */},
    {11, ColorAspects::TransferXvYCC},
    {12, ColorAspects::TransferBT1361},
    {13, ColorAspects::TransferSRGB},
    // -- ITU T.832 201201 ends here
    {14, ColorAspects::TransferSMPTE170M},
    {15, ColorAspects::TransferSMPTE170M},
    {16, ColorAspects::TransferST2084},
    {17, ColorAspects::TransferST428},
    {18, ColorAspects::TransferHLG},
}};

const static Lookup<int32_t, ColorAspects::MatrixCoeffs> sIsoMatrixCoeffs{{
    {0, ColorAspects::MatrixOther},
    {1, ColorAspects::MatrixBT709_5},
    {2, ColorAspects::MatrixUnspecified},
    {4, ColorAspects::MatrixBT470_6M},
    {6, ColorAspects::MatrixBT601_6 /* main */},
    {5, ColorAspects::MatrixBT601_6},
    {7, ColorAspects::MatrixSMPTE240M},
    {8, ColorAspects::MatrixOther /* YCgCo */},
    // -- ITU T.832 201201 ends here
    {9, ColorAspects::MatrixBT2020},
    {10, ColorAspects::MatrixBT2020Constant},
}};

// static
void ColorUtils::convertCodecColorAspectsToIsoAspects(
    const ColorAspects& aspects,
    int32_t* primaries,
    int32_t* transfer,
    int32_t* coeffs,
    bool* fullRange) {
  if (aspects.mPrimaries == ColorAspects::PrimariesOther ||
      !sIsoPrimaries.map(aspects.mPrimaries, primaries)) {
    AVE_CHECK(sIsoPrimaries.map(ColorAspects::PrimariesUnspecified, primaries));
  }
  if (aspects.mTransfer == ColorAspects::TransferOther ||
      !sIsoTransfers.map(aspects.mTransfer, transfer)) {
    AVE_CHECK(sIsoTransfers.map(ColorAspects::TransferUnspecified, transfer));
  }
  if (aspects.mMatrixCoeffs == ColorAspects::MatrixOther ||
      !sIsoMatrixCoeffs.map(aspects.mMatrixCoeffs, coeffs)) {
    AVE_CHECK(sIsoMatrixCoeffs.map(ColorAspects::MatrixUnspecified, coeffs));
  }
  *fullRange = aspects.mRange == ColorAspects::RangeFull;
}

// static
void ColorUtils::convertIsoColorAspectsToCodecAspects(int32_t primaries,
                                                      int32_t transfer,
                                                      int32_t coeffs,
                                                      bool fullRange,
                                                      ColorAspects& aspects) {
  if (!sIsoPrimaries.map(primaries, &aspects.mPrimaries)) {
    aspects.mPrimaries = ColorAspects::PrimariesUnspecified;
  }
  if (!sIsoTransfers.map(transfer, &aspects.mTransfer)) {
    aspects.mTransfer = ColorAspects::TransferUnspecified;
  }
  if (!sIsoMatrixCoeffs.map(coeffs, &aspects.mMatrixCoeffs)) {
    aspects.mMatrixCoeffs = ColorAspects::MatrixUnspecified;
  }
  aspects.mRange =
      fullRange ? ColorAspects::RangeFull : ColorAspects::RangeLimited;
}

void ColorUtils::convertIsoColorAspectsToPlatformAspects(int32_t primaries,
                                                         int32_t transfer,
                                                         int32_t coeffs,
                                                         bool fullRange,
                                                         int32_t* range,
                                                         int32_t* standard,
                                                         int32_t* outtransfer) {
  ColorAspects aspects = {};
  convertIsoColorAspectsToCodecAspects(primaries, transfer, coeffs, fullRange,
                                       aspects);
  convertCodecColorAspectsToPlatformAspects(aspects, range, standard,
                                            outtransfer);
}

// static
ColorAspects ColorUtils::unpackToColorAspects(uint32_t packed) {
  ColorAspects aspects = {};
  aspects.mRange = static_cast<ColorAspects::Range>((packed >> 24) & 0xFF);
  aspects.mPrimaries =
      static_cast<ColorAspects::Primaries>((packed >> 16) & 0xFF);
  aspects.mMatrixCoeffs =
      static_cast<ColorAspects::MatrixCoeffs>((packed >> 8) & 0xFF);
  aspects.mTransfer = static_cast<ColorAspects::Transfer>(packed & 0xFF);

  return aspects;
}

// static
uint32_t ColorUtils::packToU32(const ColorAspects& aspects) {
  return (aspects.mRange << 24) | (aspects.mPrimaries << 16) |
         (aspects.mMatrixCoeffs << 8) | aspects.mTransfer;
}

// static
void ColorUtils::setDefaultCodecColorAspectsIfNeeded(ColorAspects& aspects,
                                                     int32_t width,
                                                     int32_t height) {
  ColorAspects::MatrixCoeffs coeffs = ColorAspects::MatrixUnspecified;
  ColorAspects::Primaries primaries = ColorAspects::PrimariesUnspecified;

  // Default to BT2020, BT709 or BT601 based on size. Allow 2.35:1 aspect ratio.
  // Limit BT601 to PAL or smaller, BT2020 to 4K or larger, leaving BT709 for
  // all resolutions in between.
  if (width >= 3840 || height >= 3840 ||
      width * static_cast<int64_t>(height) >= 3840 * 1634) {
    primaries = ColorAspects::PrimariesBT2020;
    coeffs = ColorAspects::MatrixBT2020;
  } else if ((width <= 720 && height > 480 && height <= 576) ||
             (height <= 720 && width > 480 && width <= 576)) {
    primaries = ColorAspects::PrimariesBT601_6_625;
    coeffs = ColorAspects::MatrixBT601_6;
  } else if ((width <= 720 && height <= 480) ||
             (height <= 720 && width <= 480)) {
    primaries = ColorAspects::PrimariesBT601_6_525;
    coeffs = ColorAspects::MatrixBT601_6;
  } else {
    primaries = ColorAspects::PrimariesBT709_5;
    coeffs = ColorAspects::MatrixBT709_5;
  }

  if (aspects.mRange == ColorAspects::RangeUnspecified) {
    aspects.mRange = ColorAspects::RangeLimited;
  }

  if (aspects.mPrimaries == ColorAspects::PrimariesUnspecified) {
    aspects.mPrimaries = primaries;
  }
  if (aspects.mMatrixCoeffs == ColorAspects::MatrixUnspecified) {
    aspects.mMatrixCoeffs = coeffs;
  }
  if (aspects.mTransfer == ColorAspects::TransferUnspecified) {
    aspects.mTransfer = ColorAspects::TransferSMPTE170M;
  }
}

#if 0

// TODO: move this into a Video HAL
const static Lookup<CU::ColorStandard,
                    std::pair<CA::Primaries, CA::MatrixCoeffs>>
    sStandardFallbacks{{
        {CU::kColorStandardBT601_625,
         {CA::PrimariesBT709_5, CA::MatrixBT470_6M}},
        {CU::kColorStandardBT601_625,
         {CA::PrimariesBT709_5, CA::MatrixBT601_6}},
        {CU::kColorStandardBT709, {CA::PrimariesBT709_5, CA::MatrixSMPTE240M}},
        {CU::kColorStandardBT709, {CA::PrimariesBT709_5, CA::MatrixBT2020}},
        {CU::kColorStandardBT601_525,
         {CA::PrimariesBT709_5, CA::MatrixBT2020Constant}},

        {CU::kColorStandardBT2020Constant,
         {CA::PrimariesBT470_6M, CA::MatrixBT2020Constant}},

        {CU::kColorStandardBT601_625,
         {CA::PrimariesBT601_6_625, CA::MatrixBT470_6M}},
        {CU::kColorStandardBT601_525,
         {CA::PrimariesBT601_6_625, CA::MatrixBT2020Constant}},

        {CU::kColorStandardBT601_525,
         {CA::PrimariesBT601_6_525, CA::MatrixBT470_6M}},
        {CU::kColorStandardBT601_525,
         {CA::PrimariesBT601_6_525, CA::MatrixBT2020Constant}},

        {CU::kColorStandardBT2020Constant,
         {CA::PrimariesGenericFilm, CA::MatrixBT2020Constant}},
    }};

const static Lookup<CU::ColorStandard, CA::Primaries>
    sStandardPrimariesFallbacks{{
        {CU::kColorStandardFilm, CA::PrimariesGenericFilm},
        {CU::kColorStandardBT470M, CA::PrimariesBT470_6M},
        {CU::kColorStandardBT2020, CA::PrimariesBT2020},
        {CU::kColorStandardBT601_525_Unadjusted, CA::PrimariesBT601_6_525},
        {CU::kColorStandardBT601_625_Unadjusted, CA::PrimariesBT601_6_625},
    }};

const static Lookup<android_dataspace, android_dataspace> sLegacyDataSpaceToV0{{
    {HAL_DATASPACE_SRGB, HAL_DATASPACE_V0_SRGB},
    {HAL_DATASPACE_BT709, HAL_DATASPACE_V0_BT709},
    {HAL_DATASPACE_SRGB_LINEAR, HAL_DATASPACE_V0_SRGB_LINEAR},
    {HAL_DATASPACE_BT601_525, HAL_DATASPACE_V0_BT601_525},
    {HAL_DATASPACE_BT601_625, HAL_DATASPACE_V0_BT601_625},
    {HAL_DATASPACE_JFIF, HAL_DATASPACE_V0_JFIF},
}};

#define GET_HAL_ENUM(class, name) HAL_DATASPACE_##class##name
#define GET_HAL_BITFIELD(class, name) \
  (GET_HAL_ENUM(class, _##name) >> GET_HAL_ENUM(class, _SHIFT))

const static Lookup<CU::ColorStandard, uint32_t> sGfxStandards{{
    {CU::kColorStandardUnspecified, GET_HAL_BITFIELD(STANDARD, UNSPECIFIED)},
    {CU::kColorStandardBT709, GET_HAL_BITFIELD(STANDARD, BT709)},
    {CU::kColorStandardBT601_625, GET_HAL_BITFIELD(STANDARD, BT601_625)},
    {CU::kColorStandardBT601_625_Unadjusted,
     GET_HAL_BITFIELD(STANDARD, BT601_625_UNADJUSTED)},
    {CU::kColorStandardBT601_525, GET_HAL_BITFIELD(STANDARD, BT601_525)},
    {CU::kColorStandardBT601_525_Unadjusted,
     GET_HAL_BITFIELD(STANDARD, BT601_525_UNADJUSTED)},
    {CU::kColorStandardBT2020, GET_HAL_BITFIELD(STANDARD, BT2020)},
    {CU::kColorStandardBT2020Constant,
     GET_HAL_BITFIELD(STANDARD, BT2020_CONSTANT_LUMINANCE)},
    {CU::kColorStandardBT470M, GET_HAL_BITFIELD(STANDARD, BT470M)},
    {CU::kColorStandardFilm, GET_HAL_BITFIELD(STANDARD, FILM)},
    {CU::kColorStandardDCI_P3, GET_HAL_BITFIELD(STANDARD, DCI_P3)},
}};

// verify public values are stable
static_assert(CU::kColorStandardUnspecified == 0, "SDK mismatch");  // N
static_assert(CU::kColorStandardBT709 == 1, "SDK mismatch");        // N
static_assert(CU::kColorStandardBT601_625 == 2, "SDK mismatch");    // N
static_assert(CU::kColorStandardBT601_525 == 4, "SDK mismatch");    // N
static_assert(CU::kColorStandardBT2020 == 6, "SDK mismatch");       // N

const static Lookup<CU::ColorTransfer, uint32_t> sGfxTransfers{{
    {CU::kColorTransferUnspecified, GET_HAL_BITFIELD(TRANSFER, UNSPECIFIED)},
    {CU::kColorTransferLinear, GET_HAL_BITFIELD(TRANSFER, LINEAR)},
    {CU::kColorTransferSRGB, GET_HAL_BITFIELD(TRANSFER, SRGB)},
    {CU::kColorTransferSMPTE_170M, GET_HAL_BITFIELD(TRANSFER, SMPTE_170M)},
    {CU::kColorTransferGamma22, GET_HAL_BITFIELD(TRANSFER, GAMMA2_2)},
    {CU::kColorTransferGamma28, GET_HAL_BITFIELD(TRANSFER, GAMMA2_8)},
    {CU::kColorTransferST2084, GET_HAL_BITFIELD(TRANSFER, ST2084)},
    {CU::kColorTransferHLG, GET_HAL_BITFIELD(TRANSFER, HLG)},
}};

// verify public values are stable
static_assert(CU::kColorTransferUnspecified == 0, "SDK mismatch");  // N
static_assert(CU::kColorTransferLinear == 1, "SDK mismatch");       // N
static_assert(CU::kColorTransferSRGB == 2, "SDK mismatch");         // N
static_assert(CU::kColorTransferSMPTE_170M == 3, "SDK mismatch");   // N
static_assert(CU::kColorTransferST2084 == 6, "SDK mismatch");       // N
static_assert(CU::kColorTransferHLG == 7, "SDK mismatch");          // N

const static Lookup<CU::ColorRange, uint32_t> sGfxRanges{{
    {CU::kColorRangeUnspecified, GET_HAL_BITFIELD(RANGE, UNSPECIFIED)},
    {CU::kColorRangeFull, GET_HAL_BITFIELD(RANGE, FULL)},
    {CU::kColorRangeLimited, GET_HAL_BITFIELD(RANGE, LIMITED)},
}};

// verify public values are stable
static_assert(CU::kColorRangeUnspecified == 0, "SDK mismatch");  // N
static_assert(CU::kColorRangeFull == 1, "SDK mismatch");         // N
static_assert(CU::kColorRangeLimited == 2, "SDK mismatch");      // N

#undef GET_HAL_BITFIELD
#undef GET_HAL_ENUM

#endif
// bool ColorUtils::convertDataSpaceToV0(android_dataspace& dataSpace) {
//  (void)sLegacyDataSpaceToV0.lookup(dataSpace, &dataSpace);
//  return (dataSpace & 0xC000FFFF) == 0;
//}

bool ColorUtils::checkIfAspectsChangedAndUnspecifyThem(
    ColorAspects& aspects,
    const ColorAspects& orig,
    bool usePlatformAspects) {
  // remove changed aspects (change them to Unspecified)
  bool changed = false;
  if (aspects.mRange && aspects.mRange != orig.mRange) {
    aspects.mRange = ColorAspects::RangeUnspecified;
    changed = true;
  }
  if (aspects.mPrimaries && aspects.mPrimaries != orig.mPrimaries) {
    aspects.mPrimaries = ColorAspects::PrimariesUnspecified;
    if (usePlatformAspects) {
      aspects.mMatrixCoeffs = ColorAspects::MatrixUnspecified;
    }
    changed = true;
  }
  if (aspects.mMatrixCoeffs && aspects.mMatrixCoeffs != orig.mMatrixCoeffs) {
    aspects.mMatrixCoeffs = ColorAspects::MatrixUnspecified;
    if (usePlatformAspects) {
      aspects.mPrimaries = ColorAspects::PrimariesUnspecified;
    }
    changed = true;
  }
  if (aspects.mTransfer && aspects.mTransfer != orig.mTransfer) {
    aspects.mTransfer = ColorAspects::TransferUnspecified;
    changed = true;
  }
  return changed;
}

// static
// TODO(youfa) android_dataspace
// android_dataspace ColorUtils::getDataSpaceForColorAspects(ColorAspects&
// aspects,
//                                                          bool mayExpand) {
//  // This platform implementation never expands color space (e.g. returns an
//  // expanded dataspace to use where the codec does in-the-background color
//  // space conversion)
//  mayExpand = false;
//
//  if (aspects.mRange == ColorAspects::RangeUnspecified ||
//      aspects.mPrimaries == ColorAspects::PrimariesUnspecified ||
//      aspects.mMatrixCoeffs == ColorAspects::MatrixUnspecified ||
//      aspects.mTransfer == ColorAspects::TransferUnspecified) {
//    ALOGW("expected specified color aspects (%u:%u:%u:%u)", aspects.mRange,
//          aspects.mPrimaries, aspects.mMatrixCoeffs, aspects.mTransfer);
//  }
//
//  // default to video range and transfer
//  ColorRange range = kColorRangeLimited;
//  ColorTransfer transfer = kColorTransferSMPTE_170M;
//  (void)sRanges.map(aspects.mRange, &range);
//  (void)sTransfers.map(aspects.mTransfer, &transfer);
//
//  ColorStandard standard = kColorStandardBT709;
//  auto pair = std::make_pair(aspects.mPrimaries, aspects.mMatrixCoeffs);
//  if (!sStandards.map(pair, &standard)) {
//    if (!sStandardFallbacks.map(pair, &standard)) {
//      (void)sStandardPrimariesFallbacks.map(aspects.mPrimaries, &standard);
//
//      if (aspects.mMatrixCoeffs == ColorAspects::MatrixBT2020Constant) {
//        range = kColorRangeFull;
//      }
//    }
//  }
//
//  // assume 1-to-1 mapping to HAL values (to deal with potential vendor
//  // extensions)
//  uint32_t gfxRange = range;
//  uint32_t gfxStandard = standard;
//  uint32_t gfxTransfer = transfer;
//  // TRICKY: use & to ensure all three mappings are completed
//  if (!(sGfxRanges.map(range, &gfxRange) &
//        sGfxStandards.map(standard, &gfxStandard) &
//        sGfxTransfers.map(transfer, &gfxTransfer))) {
//    ALOGW(
//        "could not safely map platform color aspects (R:%u(%s) S:%u(%s) "
//        "T:%u(%s) to "
//        "graphics dataspace (R:%u S:%u T:%u)",
//        range, asString(range), standard, asString(standard), transfer,
//        asString(transfer), gfxRange, gfxStandard, gfxTransfer);
//  }
//
//  android_dataspace dataSpace =
//      (android_dataspace)((gfxRange << HAL_DATASPACE_RANGE_SHIFT) |
//                          (gfxStandard << HAL_DATASPACE_STANDARD_SHIFT) |
//                          (gfxTransfer << HAL_DATASPACE_TRANSFER_SHIFT));
//  (void)sLegacyDataSpaceToV0.rlookup(dataSpace, &dataSpace);
//
//  if (!mayExpand) {
//    // update codec aspects based on dataspace
//    convertPlatformColorAspectsToCodecAspects(range, standard, transfer,
//                                              aspects);
//  }
//  return dataSpace;
//}
//
//// static
// void ColorUtils::getColorConfigFromDataSpace(const android_dataspace&
// dataspace,
//                                             int32_t* range,
//                                             int32_t* standard,
//                                             int32_t* transfer) {
//  uint32_t gfxRange =
//      (dataspace & HAL_DATASPACE_RANGE_MASK) >> HAL_DATASPACE_RANGE_SHIFT;
//  uint32_t gfxStandard =
//      (dataspace & HAL_DATASPACE_STANDARD_MASK) >>
//      HAL_DATASPACE_STANDARD_SHIFT;
//  uint32_t gfxTransfer =
//      (dataspace & HAL_DATASPACE_TRANSFER_MASK) >>
//      HAL_DATASPACE_TRANSFER_SHIFT;
//
//  // assume 1-to-1 mapping to HAL values (to deal with potential vendor
//  // extensions)
//  CU::ColorRange cuRange = CU::kColorRangeUnspecified;
//  CU::ColorStandard cuStandard = CU::kColorStandardUnspecified;
//  CU::ColorTransfer cuTransfer = CU::kColorTransferUnspecified;
//  // TRICKY: use & to ensure all three mappings are completed
//  if (!(sGfxRanges.map(gfxRange, &cuRange) &
//        sGfxStandards.map(gfxStandard, &cuStandard) &
//        sGfxTransfers.map(gfxTransfer, &cuTransfer))) {
//    ALOGW(
//        "could not safely map graphics dataspace (R:%u S:%u T:%u) to "
//        "platform color aspects (R:%u(%s) S:%u(%s) T:%u(%s)",
//        gfxRange, gfxStandard, gfxTransfer, cuRange, asString(cuRange),
//        cuStandard, asString(cuStandard), cuTransfer, asString(cuTransfer));
//  }
//  *range = cuRange;
//  *standard = cuStandard;
//  *transfer = cuTransfer;
//}

// static
void ColorUtils::getColorConfigFromFormat(
    const std::shared_ptr<Message>& format,
    int32_t* range,
    int32_t* standard,
    int32_t* transfer) {
  if (!format->findInt32("color-range", range)) {
    *range = static_cast<int32_t>(kColorRangeUnspecified);
  }
  if (!format->findInt32("color-standard", standard)) {
    *standard = static_cast<int32_t>(kColorStandardUnspecified);
  }
  if (!format->findInt32("color-transfer", transfer)) {
    *transfer = static_cast<int32_t>(kColorTransferUnspecified);
  }
}

// static
void ColorUtils::copyColorConfig(const std::shared_ptr<Message>& source,
                                 std::shared_ptr<Message>& target) {
  // 0 values are unspecified
  int32_t value = 0;
  if (source->findInt32("color-range", &value)) {
    target->setInt32("color-range", value);
  }
  if (source->findInt32("color-standard", &value)) {
    target->setInt32("color-standard", value);
  }
  if (source->findInt32("color-transfer", &value)) {
    target->setInt32("color-transfer", value);
  }
}

// static
void ColorUtils::getColorAspectsFromFormat(
    const std::shared_ptr<Message>& format,
    ColorAspects& aspects) {
  int32_t range = 0, standard = 0, transfer = 0;
  getColorConfigFromFormat(format, &range, &standard, &transfer);

  if (convertPlatformColorAspectsToCodecAspects(range, standard, transfer,
                                                aspects) != OK) {
    AVE_LOG(LS_WARNING) << "Ignoring illegal color aspects(R:" << range << "("
                        << asString(static_cast<ColorRange>(range))
                        << "), S:" << standard << "("
                        << asString(static_cast<ColorStandard>(standard))
                        << "), T:" << transfer << "("
                        << asString(static_cast<ColorTransfer>(transfer))
                        << "))";
    // Invalid values were converted to unspecified !params!, but otherwise were
    // not changed For encoders, we leave these as is. For decoders, we will use
    // default values.
  }
  AVE_LOG(LS_VERBOSE) << "Got color aspects (R:" << aspects.mRange << "("
                      << asString(aspects.mRange)
                      << "), P:" << aspects.mPrimaries << "("
                      << asString(aspects.mPrimaries)
                      << "), M:" << aspects.mMatrixCoeffs << "("
                      << asString(aspects.mMatrixCoeffs)
                      << "), T:" << aspects.mTransfer << "("
                      << asString(aspects.mTransfer) << ")) "
                      << "from format (out:R:" << range << "("
                      << asString(static_cast<ColorRange>(range))
                      << "), S:" << standard << "("
                      << asString(static_cast<ColorStandard>(standard))
                      << "), T:" << transfer << "("
                      << asString(static_cast<ColorTransfer>(transfer)) << "))";
}

// static
void ColorUtils::setColorAspectsIntoFormat(const ColorAspects& aspects,
                                           std::shared_ptr<Message>& format,
                                           bool force) {
  int32_t range = 0, standard = 0, transfer = 0;
  convertCodecColorAspectsToPlatformAspects(aspects, &range, &standard,
                                            &transfer);
  // save set values to base output format
  // (encoder input format will read back actually supported values by the
  // codec)
  if (range != 0 || force) {
    format->setInt32("color-range", range);
  }
  if (standard != 0 || force) {
    format->setInt32("color-standard", standard);
  }
  if (transfer != 0 || force) {
    format->setInt32("color-transfer", transfer);
  }
  AVE_LOG(LS_VERBOSE) << "setting color aspects (R:" << aspects.mRange << "("
                      << asString(aspects.mRange)
                      << "), P:" << aspects.mPrimaries << "("
                      << asString(aspects.mPrimaries)
                      << "), M:" << aspects.mMatrixCoeffs << "("
                      << asString(aspects.mMatrixCoeffs)
                      << "), T:" << aspects.mTransfer << "("
                      << asString(aspects.mTransfer) << ")) "
                      << "from format (out:R:" << range << "("
                      << asString((ColorRange)range) << "), S:" << standard
                      << "(" << asString((ColorStandard)standard)
                      << "), T:" << transfer << "("
                      << asString((ColorTransfer)transfer) << "))";
}
// static
void ColorUtils::fillHdrStaticInfoBuffer(const HDRStaticInfo& info,
                                         uint8_t* data) {
  // Static_Metadata_Descriptor_ID
  data[0] = info.mID;

  // display primary 0
  data[1] = LO_UINT16(info.sType1.mR.x);
  data[2] = HI_UINT16(info.sType1.mR.x);
  data[3] = LO_UINT16(info.sType1.mR.y);
  data[4] = HI_UINT16(info.sType1.mR.y);

  // display primary 1
  data[5] = LO_UINT16(info.sType1.mG.x);
  data[6] = HI_UINT16(info.sType1.mG.x);
  data[7] = LO_UINT16(info.sType1.mG.y);
  data[8] = HI_UINT16(info.sType1.mG.y);

  // display primary 2
  data[9] = LO_UINT16(info.sType1.mB.x);
  data[10] = HI_UINT16(info.sType1.mB.x);
  data[11] = LO_UINT16(info.sType1.mB.y);
  data[12] = HI_UINT16(info.sType1.mB.y);

  // white point
  data[13] = LO_UINT16(info.sType1.mW.x);
  data[14] = HI_UINT16(info.sType1.mW.x);
  data[15] = LO_UINT16(info.sType1.mW.y);
  data[16] = HI_UINT16(info.sType1.mW.y);

  // MaxDisplayLuminance
  data[17] = LO_UINT16(info.sType1.mMaxDisplayLuminance);
  data[18] = HI_UINT16(info.sType1.mMaxDisplayLuminance);

  // MinDisplayLuminance
  data[19] = LO_UINT16(info.sType1.mMinDisplayLuminance);
  data[20] = HI_UINT16(info.sType1.mMinDisplayLuminance);

  // MaxContentLightLevel
  data[21] = LO_UINT16(info.sType1.mMaxContentLightLevel);
  data[22] = HI_UINT16(info.sType1.mMaxContentLightLevel);

  // MaxFrameAverageLightLevel
  data[23] = LO_UINT16(info.sType1.mMaxFrameAverageLightLevel);
  data[24] = HI_UINT16(info.sType1.mMaxFrameAverageLightLevel);
}

// static
void ColorUtils::setHDRStaticInfoIntoFormat(const HDRStaticInfo& info,
                                            std::shared_ptr<Message>& format) {
  auto infoBuffer = std::make_shared<Buffer>(25);

  // Convert the data in infoBuffer to little endian format as defined by
  // CTA-861-3
  uint8_t* data = infoBuffer->data();
  fillHdrStaticInfoBuffer(info, data);

  format->setBuffer("hdr-static-info", infoBuffer);
}

// a simple method copied from Utils.cpp
static uint16_t U16LE_AT(const uint8_t* ptr) {
  return ptr[0] | (ptr[1] << 8);
}

// static
bool ColorUtils::getHDRStaticInfoFromFormat(
    const std::shared_ptr<Message>& format,
    HDRStaticInfo* info) {
  std::shared_ptr<Buffer> buf;
  if (!format->findBuffer("hdr-static-info", buf)) {
    return false;
  }

  // TODO: Make this more flexible when adding more members to HDRStaticInfo
  if (buf->size() != 25 /* static Metadata Type 1 size */) {
    AVE_LOG(LS_WARNING) << "Ignore invalid HDRStaticInfo with size: "
                        << buf->size();
    return false;
  }

  const uint8_t* data = buf->data();
  if (*data != HDRStaticInfo::kType1) {
    AVE_LOG(LS_WARNING) << "Unsupported static Metadata Type " << *data;
    return false;
  }

  info->mID = HDRStaticInfo::kType1;
  info->sType1.mR.x = U16LE_AT(&data[1]);
  info->sType1.mR.y = U16LE_AT(&data[3]);
  info->sType1.mG.x = U16LE_AT(&data[5]);
  info->sType1.mG.y = U16LE_AT(&data[7]);
  info->sType1.mB.x = U16LE_AT(&data[9]);
  info->sType1.mB.y = U16LE_AT(&data[11]);
  info->sType1.mW.x = U16LE_AT(&data[13]);
  info->sType1.mW.y = U16LE_AT(&data[15]);
  info->sType1.mMaxDisplayLuminance = U16LE_AT(&data[17]);
  info->sType1.mMinDisplayLuminance = U16LE_AT(&data[19]);
  info->sType1.mMaxContentLightLevel = U16LE_AT(&data[21]);
  info->sType1.mMaxFrameAverageLightLevel = U16LE_AT(&data[23]);

  AVE_LOG(LS_VERBOSE) << "Got HDRStaticInfo from config (R: "
                      << info->sType1.mR.x << " " << info->sType1.mR.y
                      << ", G: " << info->sType1.mG.x << " "
                      << info->sType1.mG.y << ", B: " << info->sType1.mB.x
                      << " " << info->sType1.mB.y
                      << ", W: " << info->sType1.mW.x << " "
                      << info->sType1.mW.y
                      << ", MaxDispL: " << info->sType1.mMaxDisplayLuminance
                      << ", MinDispL: " << info->sType1.mMinDisplayLuminance
                      << ", MaxContentL: " << info->sType1.mMaxContentLightLevel
                      << ", MaxFrameAvgL: "
                      << info->sType1.mMaxFrameAverageLightLevel << ")";

  return true;
}

// static
bool ColorUtils::isHDRStaticInfoValid(HDRStaticInfo* info) {
  if (info->sType1.mMaxDisplayLuminance > 0 &&
      info->sType1.mMinDisplayLuminance > 0) {
    return true;
  }
  if (info->sType1.mMaxContentLightLevel > 0 &&
      info->sType1.mMaxFrameAverageLightLevel > 0) {
    return true;
  }
  return false;
}

}  // namespace media
}  // namespace ave
