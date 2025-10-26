/*
 * simple_passthrough_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "simple_passthrough_codec.h"

#include "base/logging.h"
#include "base/sequence_checker.h"

namespace ave {
namespace media {

namespace {
const size_t kInvalidIndex = static_cast<size_t>(-1);
}  // namespace

SimplePassthroughCodec::SimplePassthroughCodec(bool is_encoder)
    : SimpleCodec(is_encoder), frame_count_(0) {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec created, is_encoder: "
                   << is_encoder;
}

SimplePassthroughCodec::~SimplePassthroughCodec() {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec destroyed, processed "
                   << frame_count_ << " frames";
}

status_t SimplePassthroughCodec::OnConfigure(
    const std::shared_ptr<CodecConfig>& config) {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec::OnConfigure";

  // Passthrough codec doesn't care about format, just accept any config
  if (!config || !config->format) {
    return BAD_VALUE;
  }

  frame_count_ = 0;
  return OK;
}

status_t SimplePassthroughCodec::OnStart() {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec::OnStart";
  return OK;
}

status_t SimplePassthroughCodec::OnStop() {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec::OnStop";
  return OK;
}

status_t SimplePassthroughCodec::OnReset() {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec::OnReset";
  frame_count_ = 0;
  return OK;
}

status_t SimplePassthroughCodec::OnFlush() {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec::OnFlush";
  return OK;
}

status_t SimplePassthroughCodec::OnRelease() {
  AVE_LOG(LS_INFO) << "SimplePassthroughCodec::OnRelease, total frames: "
                   << frame_count_;
  return OK;
}

void SimplePassthroughCodec::ProcessInput(size_t index) {
  AVE_LOG(LS_VERBOSE) << "SimplePassthroughCodec::ProcessInput(" << index
                      << ")";

  std::lock_guard<std::mutex> lock(lock_);

  // Validate input buffer
  if (index >= input_buffers_.size() || !input_buffers_[index].in_use) {
    AVE_LOG(LS_WARNING) << "Invalid input buffer index: " << index;
    return;
  }

  auto& input_buffer = input_buffers_[index].buffer;
  size_t input_size = input_buffer->size();

  AVE_LOG(LS_VERBOSE) << "Input buffer size: " << input_size;

  // Find available output buffer
  size_t output_index = GetAvailableOutputBufferIndex();
  if (output_index == kInvalidIndex) {
    AVE_LOG(LS_WARNING) << "No available output buffer";
    // Keep input buffer occupied, will retry later
    return;
  }

  // Get output buffer
  auto& output_buffer = output_buffers_[output_index].buffer;

  // Passthrough: simply copy data from input to output
  output_buffer->EnsureCapacity(input_size, false);
  if (input_size > 0) {
    std::memcpy(output_buffer->data(), input_buffer->data(), input_size);
  }
  output_buffer->SetRange(0, input_size);

  // Copy metadata if present
  if (input_buffer->format()) {
    output_buffer->format() = input_buffer->format();
  }

  // Release input buffer
  input_buffers_[index].in_use = false;
  cv_.notify_all();

  // Push output buffer to queue
  PushOutputBuffer(output_index);

  frame_count_++;

  AVE_LOG(LS_VERBOSE) << "Passthrough frame " << frame_count_
                      << ", size: " << input_size;

  // Notify that input buffer is available again
  NotifyInputBufferAvailable(index);
  // Notify that output buffer is ready
  NotifyOutputBufferAvailable(output_index);
}

void SimplePassthroughCodec::ProcessOutput() {
  // For passthrough codec, output is generated immediately in ProcessInput
  // So this method doesn't need to do anything
  AVE_LOG(LS_VERBOSE) << "SimplePassthroughCodec::ProcessOutput (no-op)";
}

}  // namespace media
}  // namespace ave
