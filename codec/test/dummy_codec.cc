/*
 * dummy_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "dummy_codec.h"

#include "base/logging.h"
#include "base/task_util/default_task_runner_factory.h"

namespace ave {
namespace media {
namespace test {

namespace {
const int kMaxInputBuffers = 5;
const int kMaxOutputBuffers = 4;
const size_t kDefaultBufferSize = 1024 * 1024;  // 1MB
}  // namespace

DummyCodec::DummyCodec(bool is_encoder)
    : is_encoder_(is_encoder),
      task_runner_(std::make_unique<base::TaskRunner>(
          base::CreateDefaultTaskRunnerFactory()->CreateTaskRunner(
              "DummyCodec",
              base::TaskRunnerFactory::Priority::NORMAL))) {
  AVE_LOG(LS_INFO) << "DummyCodec created, is_encoder: " << is_encoder_;
}

DummyCodec::~DummyCodec() {
  Stop();
  Release();
}

status_t DummyCodec::Configure(const std::shared_ptr<CodecConfig>& config) {
  std::lock_guard<std::mutex> lock(lock_);
  config_ = config;

  input_buffers_ = std::vector<BufferEntry>(kMaxInputBuffers);
  for (auto& entry : input_buffers_) {
    entry.buffer = std::make_shared<CodecBuffer>(kDefaultBufferSize);
    entry.in_use = false;
  }

  output_buffers_ = std::vector<BufferEntry>(kMaxOutputBuffers);
  for (auto& entry : output_buffers_) {
    entry.buffer = std::make_shared<CodecBuffer>(kDefaultBufferSize);
    entry.in_use = false;
  }

  return OK;
}

status_t DummyCodec::SetCallback(CodecCallback* callback) {
  std::lock_guard<std::mutex> lock(lock_);
  callback_ = callback;
  return OK;
}

status_t DummyCodec::GetInputBuffer(size_t index,
                                    std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  if (index >= input_buffers_.size()) {
    return INVALID_OPERATION;
  }
  buffer = input_buffers_[index].buffer;
  return OK;
}

status_t DummyCodec::GetOutputBuffer(size_t index,
                                     std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  if (index >= output_buffers_.size()) {
    return INVALID_OPERATION;
  }
  buffer = output_buffers_[index].buffer;
  return OK;
}

status_t DummyCodec::Start() {
  std::lock_guard<std::mutex> lock(lock_);
  if (started_) {
    return INVALID_OPERATION;
  }
  started_ = true;
  return OK;
}

status_t DummyCodec::Stop() {
  std::lock_guard<std::mutex> lock(lock_);
  started_ = false;
  return OK;
}

status_t DummyCodec::Reset() {
  std::lock_guard<std::mutex> lock(lock_);
  while (!input_queue_.empty()) {
    input_queue_.pop();
  }
  while (!output_queue_.empty()) {
    output_queue_.pop();
  }
  for (auto& entry : input_buffers_) {
    entry.in_use = false;
  }
  for (auto& entry : output_buffers_) {
    entry.in_use = false;
  }
  return OK;
}

status_t DummyCodec::Flush() {
  return Reset();
}

status_t DummyCodec::Release() {
  std::lock_guard<std::mutex> lock(lock_);
  input_buffers_.clear();
  output_buffers_.clear();
  while (!input_queue_.empty()) {
    input_queue_.pop();
  }
  while (!output_queue_.empty()) {
    output_queue_.pop();
  }
  return OK;
}

ssize_t DummyCodec::DequeueInputBuffer(int64_t timeout_ms) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_) {
    return -1;
  }

  for (size_t i = 0; i < input_buffers_.size(); ++i) {
    if (!input_buffers_[i].in_use) {
      input_buffers_[i].in_use = true;
      return static_cast<ssize_t>(i);
    }
  }
  return -1;
}

status_t DummyCodec::QueueInputBuffer(size_t index) {
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (!started_ || index >= input_buffers_.size() ||
        !input_buffers_[index].in_use) {
      return INVALID_OPERATION;
    }
    input_queue_.push(index);
  }

  // Schedule processing
  task_runner_->PostTask([this, index]() { ProcessBuffer(index); });
  return OK;
}

void DummyCodec::ProcessBuffer(size_t input_index) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_ || input_index >= input_buffers_.size()) {
    return;
  }

  auto& input_entry = input_buffers_[input_index];
  if (!input_entry.in_use) {
    return;
  }

  // Find available output buffer
  size_t output_index = static_cast<size_t>(-1);
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    if (!output_buffers_[i].in_use) {
      output_index = i;
      break;
    }
  }

  if (output_index == static_cast<size_t>(-1)) {
    return;
  }

  // Copy input to output (passthrough)
  auto& output_buffer = output_buffers_[output_index].buffer;
  auto& input_buffer = input_entry.buffer;
  size_t data_size = input_buffer->size();
  output_buffer->EnsureCapacity(data_size, false);
  if (data_size > 0) {
    memcpy(output_buffer->data(), input_buffer->data(), data_size);
  }
  output_buffer->SetRange(0, data_size);

  // Release input buffer
  input_entry.in_use = false;

  // Remove from input queue
  if (!input_queue_.empty() && input_queue_.front() == input_index) {
    input_queue_.pop();
  }

  // Queue output buffer
  output_buffers_[output_index].in_use = true;
  output_queue_.push(output_index);
  cv_.notify_all();

  // Notify callbacks
  if (callback_) {
    callback_->OnInputBufferAvailable(input_index);
    callback_->OnOutputBufferAvailable(output_index);
  }
}

ssize_t DummyCodec::DequeueOutputBuffer(int64_t timeout_ms) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_ || output_queue_.empty()) {
    return -1;
  }

  size_t index = output_queue_.front();
  output_queue_.pop();
  return static_cast<ssize_t>(index);
}

status_t DummyCodec::ReleaseOutputBuffer(size_t index, bool render) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_ || index >= output_buffers_.size() ||
      !output_buffers_[index].in_use) {
    return INVALID_OPERATION;
  }

  output_buffers_[index].in_use = false;
  cv_.notify_all();
  return OK;
}

}  // namespace test
}  // namespace media
}  // namespace ave
