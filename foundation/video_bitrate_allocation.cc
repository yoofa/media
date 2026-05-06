/*
 * video_bitrate_allocation.cc - Video bitrate allocation implementation
 *
 * Copyright (c) 2025 The AveStart project authors. All Rights Reserved.
 *
 * Ported from WebRTC project.
 */

#include "media/foundation/video_bitrate_allocation.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/checks.h"
#include "media/foundation/video_codec_constants.h"

namespace ave {
namespace media {

VideoBitrateAllocation::VideoBitrateAllocation()
    : sum_(0), is_bw_limited_(false) {}

bool VideoBitrateAllocation::SetBitrate(size_t spatial_index,
                                        size_t temporal_index,
                                        uint32_t bitrate_bps) {
  AVE_CHECK_LT(spatial_index, static_cast<size_t>(kMaxSpatialLayers));
  AVE_CHECK_LT(temporal_index, static_cast<size_t>(kMaxTemporalStreams));
  int64_t new_bitrate_sum_bps = sum_;
  std::optional<uint32_t>& layer_bitrate =
      bitrates_[spatial_index][temporal_index];
  if (layer_bitrate) {
    AVE_DCHECK_LE(*layer_bitrate, sum_);
    new_bitrate_sum_bps -= *layer_bitrate;
  }
  new_bitrate_sum_bps += bitrate_bps;
  if (std::cmp_greater(new_bitrate_sum_bps, kMaxBitrateBps)) {
    return false;
  }

  layer_bitrate = bitrate_bps;
  sum_ = static_cast<uint32_t>(new_bitrate_sum_bps);
  return true;
}

bool VideoBitrateAllocation::HasBitrate(size_t spatial_index,
                                        size_t temporal_index) const {
  AVE_CHECK_LT(spatial_index, static_cast<size_t>(kMaxSpatialLayers));
  AVE_CHECK_LT(temporal_index, static_cast<size_t>(kMaxTemporalStreams));
  return bitrates_[spatial_index][temporal_index].has_value();
}

uint32_t VideoBitrateAllocation::GetBitrate(size_t spatial_index,
                                            size_t temporal_index) const {
  AVE_CHECK_LT(spatial_index, static_cast<size_t>(kMaxSpatialLayers));
  AVE_CHECK_LT(temporal_index, static_cast<size_t>(kMaxTemporalStreams));
  return bitrates_[spatial_index][temporal_index].value_or(0);
}

// Whether the specific spatial layers has the bitrate set in any of its
// temporal layers.
bool VideoBitrateAllocation::IsSpatialLayerUsed(size_t spatial_index) const {
  AVE_CHECK_LT(spatial_index, static_cast<size_t>(kMaxSpatialLayers));
  for (size_t i = 0; i < kMaxTemporalStreams; ++i) {
    if (bitrates_[spatial_index][i].has_value()) {
      return true;
    }
  }
  return false;
}

// Get the sum of all the temporal layer for a specific spatial layer.
uint32_t VideoBitrateAllocation::GetSpatialLayerSum(
    size_t spatial_index) const {
  AVE_CHECK_LT(spatial_index, static_cast<size_t>(kMaxSpatialLayers));
  return GetTemporalLayerSum(spatial_index, kMaxTemporalStreams - 1);
}

uint32_t VideoBitrateAllocation::GetTemporalLayerSum(
    size_t spatial_index,
    size_t temporal_index) const {
  AVE_CHECK_LT(spatial_index, static_cast<size_t>(kMaxSpatialLayers));
  AVE_CHECK_LT(temporal_index, static_cast<size_t>(kMaxTemporalStreams));
  uint32_t sum = 0;
  for (size_t i = 0; i <= temporal_index; ++i) {
    sum += bitrates_[spatial_index][i].value_or(0);
  }
  return sum;
}

std::vector<uint32_t> VideoBitrateAllocation::GetTemporalLayerAllocation(
    size_t spatial_index) const {
  AVE_CHECK_LT(spatial_index, static_cast<size_t>(kMaxSpatialLayers));
  std::vector<uint32_t> temporal_rates;

  // Find the highest temporal layer with a defined bitrate in order to
  // determine the size of the temporal layer allocation.
  for (size_t i = kMaxTemporalStreams; i > 0; --i) {
    if (bitrates_[spatial_index][i - 1].has_value()) {
      temporal_rates.resize(i);
      break;
    }
  }

  for (size_t i = 0; i < temporal_rates.size(); ++i) {
    temporal_rates[i] = bitrates_[spatial_index][i].value_or(0);
  }

  return temporal_rates;
}

std::vector<std::optional<VideoBitrateAllocation>>
VideoBitrateAllocation::GetSimulcastAllocations() const {
  std::vector<std::optional<VideoBitrateAllocation>> bitrates;
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    std::optional<VideoBitrateAllocation> layer_bitrate;
    if (IsSpatialLayerUsed(si)) {
      layer_bitrate = VideoBitrateAllocation();
      for (int32_t tl = 0; tl < kMaxTemporalStreams; ++tl) {
        if (HasBitrate(si, tl)) {
          layer_bitrate->SetBitrate(0, tl, GetBitrate(si, tl));
        }
      }
    }
    bitrates.push_back(layer_bitrate);
  }
  return bitrates;
}

bool VideoBitrateAllocation::operator==(
    const VideoBitrateAllocation& other) const {
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      if (bitrates_[si][ti] != other.bitrates_[si][ti]) {
        return false;
      }
    }
  }
  return true;
}

std::string VideoBitrateAllocation::ToString() const {
  if (sum_ == 0) {
    return "VideoBitrateAllocation [ [] ]";
  }

  std::ostringstream oss;
  oss << "VideoBitrateAllocation [";
  uint32_t spatial_cumulator = 0;
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    AVE_DCHECK_LE(spatial_cumulator, sum_);
    if (spatial_cumulator == sum_) {
      break;
    }

    const uint32_t layer_sum = GetSpatialLayerSum(si);
    if (layer_sum == sum_ && si == 0) {
      oss << " [";
    } else {
      if (si > 0) {
        oss << ",";
      }
      oss << '\n' << "  [";
    }
    spatial_cumulator += layer_sum;

    uint32_t temporal_cumulator = 0;
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      AVE_DCHECK_LE(temporal_cumulator, layer_sum);
      if (temporal_cumulator == layer_sum) {
        break;
      }

      if (ti > 0) {
        oss << ", ";
      }

      uint32_t bitrate = bitrates_[si][ti].value_or(0);
      oss << bitrate;
      temporal_cumulator += bitrate;
    }
    oss << "]";
  }

  AVE_DCHECK_EQ(spatial_cumulator, sum_);
  oss << " ]";
  return oss.str();
}

}  // namespace media
}  // namespace ave
