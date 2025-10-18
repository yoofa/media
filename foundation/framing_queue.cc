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

FramingQueue::FramingQueue(CodecType codec_type) : codec_type_(codec_type) {
  buffer_.reserve(65536);  // Reserve 64KB initially
}

FramingQueue::~FramingQueue() {
  Clear();
}

status_t FramingQueue::PushData(const uint8_t* data, size_t size) {
  if (!data || size == 0) {
    return INVALID_OPERATION;
  }

  // Append new data to buffer
  buffer_.insert(buffer_.end(), data, data + size);

  // Try to parse frames from the buffer
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
  while (!frames_.empty()) {
    frames_.pop();
  }
}

status_t FramingQueue::ParseH264Frame() {
  if (buffer_.empty()) {
    return E_AGAIN;
  }

  const uint8_t* data = buffer_.data();
  size_t size = buffer_.size();
  const uint8_t* nalStart = nullptr;
  size_t nalSize = 0;

  // Get next NAL unit
  status_t result = getNextNALUnit(&data, &size, &nalStart, &nalSize, false);

  if (result != OK) {
    // If we consumed some data but didn't get a complete NAL,
    // keep the remaining data in buffer
    if (data != buffer_.data()) {
      size_t consumed = data - buffer_.data();
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
    }
    return result;
  }

  // We have a complete NAL unit, create a frame
  auto frame = MediaFrame::CreateShared(nalSize, MediaType::VIDEO);
  std::memcpy(frame->data(), nalStart, nalSize);
  frame->setRange(0, nalSize);
  frames_.push(frame);

  // Remove the consumed data from buffer
  size_t consumed = (data - buffer_.data());
  buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);

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

  // Get next AAC frame
  status_t result = GetNextAACFrame(&data, &size, &frameStart, &frameSize);

  if (result == INVALID_OPERATION) {
    // Invalid frame, skip and try again
    if (data != buffer_.data()) {
      size_t consumed = data - buffer_.data();
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
    }
    return ParseAACFrame();  // Try to find next frame
  }

  if (result != OK) {
    // If we consumed some data but didn't get a complete frame,
    // keep the remaining data in buffer
    if (data != buffer_.data()) {
      size_t consumed = data - buffer_.data();
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
    }
    return result;
  }

  // We have a complete AAC frame, create a MediaFrame
  auto frame = MediaFrame::CreateShared(frameSize, MediaType::AUDIO);
  std::memcpy(frame->data(), frameStart, frameSize);
  frame->setRange(0, frameSize);

  // Parse ADTS header to set audio info
  ADTSHeader header{};
  if (ParseADTSHeader(frameStart, frameSize, &header) == OK) {
    frame->SetSampleRate(GetSamplingRate(header.sampling_freq_index));
    // Note: We would need to convert channel_config to ChannelLayout
    // For now, just set basic channel count info
  }

  frames_.push(frame);

  // Remove the consumed data from buffer
  size_t consumed = (data - buffer_.data());
  buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);

  return OK;
}

}  // namespace media
}  // namespace ave
