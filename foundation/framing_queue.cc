/*
 * framing_queue.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "framing_queue.h"
#include "aac_utils.h"
#include "avc_utils.h"
#include "base/logging.h"

namespace ave {
namespace media {

namespace {
// H.264 NAL unit types
constexpr uint8_t kNalTypeNonIdr = 1;  // Non-IDR slice (P/B frame)
constexpr uint8_t kNalTypeIdr = 5;     // IDR slice (I frame)
constexpr uint8_t kNalTypeSei = 6;     // Supplemental Enhancement Info
constexpr uint8_t kNalTypeSps = 7;     // Sequence Parameter Set
constexpr uint8_t kNalTypePps = 8;     // Picture Parameter Set
constexpr uint8_t kNalTypeAud = 9;     // Access Unit Delimiter

// Returns true if the NAL type is a VCL (Video Coding Layer) NAL,
// i.e., it contains slice data for a picture.
inline bool IsVclNal(uint8_t nal_type) {
  return nal_type == kNalTypeNonIdr || nal_type == kNalTypeIdr;
}

// Returns true if this NAL starts a new access unit (must be output before
// adding to the next access unit). Per H.264 spec, a new AU starts when:
// - An AUD NAL is seen after VCL data
// - SPS/PPS/SEI appears after VCL data (they begin the next AU's header)
// - A VCL NAL follows VCL data from a different picture
inline bool StartsNewAccessUnit(uint8_t nal_type, bool au_has_vcl) {
  if (nal_type == kNalTypeAud) {
    return au_has_vcl;  // AUD after VCL = flush current AU
  }
  if (au_has_vcl && (nal_type == kNalTypeSps || nal_type == kNalTypePps ||
                     nal_type == kNalTypeSei)) {
    return true;  // non-VCL header after slices = start of next AU
  }
  return false;
}

const uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};
}  // namespace

FramingQueue::FramingQueue(CodecType codec_type) : codec_type_(codec_type) {
  buffer_.reserve(65536);
  au_buf_.reserve(65536);
}

FramingQueue::~FramingQueue() {
  Clear();
}

status_t FramingQueue::PushData(const uint8_t* data, size_t size) {
  if (!data || size == 0) {
    return INVALID_OPERATION;
  }

  buffer_.insert(buffer_.end(), data, data + size);

  status_t result = OK;
  while (result == OK) {
    if (codec_type_ == CodecType::kH264) {
      result = ParseH264Frame();
    } else if (codec_type_ == CodecType::kAAC) {
      result = ParseAACFrame();
    } else {
      return INVALID_OPERATION;
    }
  }

  return OK;
}

std::shared_ptr<MediaFrame> FramingQueue::PopFrame() {
  if (frames_.empty()) {
    return nullptr;
  }
  auto frame = frames_.front();
  frames_.pop();
  return frame;
}

bool FramingQueue::HasFrame() const {
  return !frames_.empty();
}

size_t FramingQueue::FrameCount() const {
  return frames_.size();
}

void FramingQueue::Clear() {
  buffer_.clear();
  au_buf_.clear();
  au_has_vcl_ = false;
  while (!frames_.empty()) {
    frames_.pop();
  }
}

void FramingQueue::Flush() {
  // Emit any accumulated access unit data as the final frame.
  if (!au_buf_.empty()) {
    auto frame = MediaFrame::CreateShared(au_buf_.size(), MediaType::VIDEO);
    std::memcpy(frame->data(), au_buf_.data(), au_buf_.size());
    frame->setRange(0, au_buf_.size());
    frames_.push(frame);
    au_buf_.clear();
    au_has_vcl_ = false;
  }
}

// Parse H.264 bitstream into complete Annex-B access units.
//
// Strategy: accumulate NAL units into au_buf_ until we detect a new access
// unit boundary, then emit the accumulated buffer as one complete frame.
// A new AU boundary is detected when:
//   - An AUD NAL appears after we already have VCL data
//   - SPS/PPS appears after we already have VCL data (start of next AU)
//   - A second IDR/non-IDR VCL slice appears (multi-slice pictures are
//     treated as complete when we see SPS/PPS/AUD next; simple heuristic)
//
// Each emitted frame contains the full Annex-B AU: start codes + NAL bytes.
status_t FramingQueue::ParseH264Frame() {
  if (buffer_.empty()) {
    return E_AGAIN;
  }

  const uint8_t* data = buffer_.data();
  size_t size = buffer_.size();
  const uint8_t* nal_start = nullptr;
  size_t nal_size = 0;

  status_t result = getNextNALUnit(&data, &size, &nal_start, &nal_size, false);

  if (result != OK) {
    // Compact consumed prefix from buffer
    if (data != nullptr && data != buffer_.data()) {
      size_t consumed = data - buffer_.data();
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
    }
    return result;
  }

  if (nal_size == 0) {
    // Consume and skip empty NAL
    if (data == nullptr) {
      buffer_.clear();
    } else {
      size_t consumed = data - buffer_.data();
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
    }
    return OK;
  }

  uint8_t nal_type = nal_start[0] & 0x1f;

  // Check if this NAL starts a new access unit
  if (StartsNewAccessUnit(nal_type, au_has_vcl_) && !au_buf_.empty()) {
    // Emit the current access unit as a complete frame
    auto frame = MediaFrame::CreateShared(au_buf_.size(), MediaType::VIDEO);
    std::memcpy(frame->data(), au_buf_.data(), au_buf_.size());
    frame->setRange(0, au_buf_.size());
    frames_.push(frame);
    au_buf_.clear();
    au_has_vcl_ = false;
  }

  // Skip AUD NALs themselves (they're just delimiters, not needed in output)
  if (nal_type != kNalTypeAud) {
    // Append start code + NAL to current access unit buffer
    au_buf_.insert(au_buf_.end(), kStartCode, kStartCode + sizeof(kStartCode));
    au_buf_.insert(au_buf_.end(), nal_start, nal_start + nal_size);

    if (IsVclNal(nal_type)) {
      au_has_vcl_ = true;
    }
  }

  // Consume processed bytes from input buffer
  if (data == nullptr) {
    buffer_.clear();
  } else {
    size_t consumed = data - buffer_.data();
    buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
  }

  return OK;
}

status_t FramingQueue::ParseAACFrame() {
  if (buffer_.empty()) {
    return E_AGAIN;
  }

  const uint8_t* data = buffer_.data();
  size_t size = buffer_.size();
  const uint8_t* frameStart = nullptr;
  size_t frameSize = 0;

  status_t result = GetNextAACFrame(&data, &size, &frameStart, &frameSize);

  if (result == INVALID_OPERATION) {
    // Invalid frame, skip and try again
    if (data != buffer_.data()) {
      size_t consumed = data - buffer_.data();
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
    }
    return ParseAACFrame();
  }

  if (result != OK) {
    if (data != buffer_.data()) {
      size_t consumed = data - buffer_.data();
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
    }
    return result;
  }

  auto frame = MediaFrame::CreateShared(frameSize, MediaType::AUDIO);
  std::memcpy(frame->data(), frameStart, frameSize);
  frame->setRange(0, frameSize);

  ADTSHeader header{};
  if (ParseADTSHeader(frameStart, frameSize, &header) == OK) {
    frame->SetSampleRate(GetSamplingRate(header.sampling_freq_index));
  }

  frames_.push(frame);

  size_t consumed = data - buffer_.data();
  buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);

  return OK;
}

}  // namespace media
}  // namespace ave
