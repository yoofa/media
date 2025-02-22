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
}  // namespace

DummyCodec::DummyCodec(bool is_encoder)
    : is_encoder_(is_encoder),
      task_runner_(std::make_unique<base::TaskRunner>(
          base::CreateDefaultTaskRunnerFactory()->CreateTaskRunner(
              "DummyCodec",
              base::TaskRunnerFactory::Priority::NORMAL))) {
  AVE_LOG(LS_INFO) << "is_encoder: " << is_encoder_;
}

DummyCodec::~DummyCodec() {
  Stop();
  Release();
}

status_t DummyCodec::Configure(const std::shared_ptr<CodecConfig>& config) {
  std::lock_guard<std::mutex> lock(lock_);
  config_ = config;

  // Initialize input and output buffers
  input_buffers_ = std::vector<BufferEntry>(kMaxInputBuffers);
  for (auto& entry : input_buffers_) {
    entry.buffer = std::make_shared<CodecBuffer>(1024 * 1024);  // 1MB buffer
  }

  output_buffers_ = std::vector<BufferEntry>(kMaxOutputBuffers);
  for (auto& entry : output_buffers_) {
    entry.buffer = std::make_shared<CodecBuffer>(1024 * 1024);  // 1MB buffer
  }

  return OK;
}

status_t DummyCodec::SetCallback(CodecCallback* callback) {
  std::lock_guard<std::mutex> lock(lock_);
  callback_ = callback;
  return OK;
}

std::vector<std::shared_ptr<CodecBuffer>> DummyCodec::InputBuffers() {
  std::vector<std::shared_ptr<CodecBuffer>> buffers;
  std::lock_guard<std::mutex> lock(lock_);
  for (auto& entry : input_buffers_) {
    buffers.push_back(entry.buffer);
  }
  return buffers;
}
std::vector<std::shared_ptr<CodecBuffer>> DummyCodec::OutputBuffers() {
  std::vector<std::shared_ptr<CodecBuffer>> buffers;
  std::lock_guard<std::mutex> lock(lock_);
  for (auto& entry : output_buffers_) {
    buffers.push_back(entry.buffer);
  }
  return buffers;
}

status_t DummyCodec::GetInputBuffer(size_t index,
                                    std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  buffer = input_buffers_[index].buffer;
  return OK;
}
status_t DummyCodec::GetOutputBuffer(size_t index,
                                     std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  buffer = output_buffers_[index].buffer;
  return OK;
}

status_t DummyCodec::Start() {
  std::lock_guard<std::mutex> lock(lock_);
  if (started_) {
    return INVALID_OPERATION;
  }

  started_ = true;

  // Start requesting input buffers
  task_runner_->PostTask([this]() { RequestInputBuffer(); });

  return OK;
}

void DummyCodec::RequestInputBuffer() {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_) {
    return;
  }

  // Find an available input buffer
  for (size_t i = 0; i < input_buffers_.size(); i++) {
    if (IsBufferAvailable(input_buffers_[i])) {
      input_buffers_[i].state = BufferState::PENDING;
      if (callback_) {
        callback_->OnInputBufferAvailable(i);
      }
      break;
    }
  }

  if (started_) {
    task_runner_->PostDelayedTask([this]() { RequestInputBuffer(); }, 1);
  }
}

std::shared_ptr<CodecBuffer> DummyCodec::FindAvailableInputBuffer(
    int32_t index) {
  std::unique_lock<std::mutex> lock(lock_);
  if (!started_) {
    return nullptr;
  }

  auto find_available = [this, index]() -> std::shared_ptr<CodecBuffer> {
    if (index < 0) {
      for (size_t i = 0; i < input_buffers_.size(); i++) {
        if (IsBufferPending(input_buffers_[i])) {
          input_buffers_[i].state = BufferState::INUSE;
          input_buffers_[i].buffer->SetIndex(i);
          return input_buffers_[i].buffer;
        }
      }
    } else if (static_cast<size_t>(index) < input_buffers_.size() &&
               IsBufferPending(input_buffers_[index])) {
      input_buffers_[index].state = BufferState::INUSE;
      input_buffers_[index].buffer->SetIndex(index);
      return input_buffers_[index].buffer;
    }
    return nullptr;
  };

  return find_available();
}

std::shared_ptr<CodecBuffer> DummyCodec::FindAvailableOutputBuffer() {
  for (size_t i = 0; i < output_buffers_.size(); i++) {
    if (IsBufferAvailable(output_buffers_[i])) {
      output_buffers_[i].state = BufferState::INUSE;
      output_buffers_[i].buffer->SetIndex(i);
      return output_buffers_[i].buffer;
    }
  }
  return nullptr;
}

void DummyCodec::ProcessBuffer(size_t input_index) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_ || input_index >= input_buffers_.size()) {
    return;
  }

  auto& input_entry = input_buffers_[input_index];
  if (!IsBufferInUse(input_entry)) {
    return;
  }

  // Find available output buffer
  auto output_buffer = FindAvailableOutputBuffer();
  if (!output_buffer) {
    // No output buffer available, retry later
    task_runner_->PostDelayedTask(
        [this, input_index]() { ProcessBuffer(input_index); }, 1);
    return;
  }

  // Process input to output (simple copy for dummy codec)
  size_t output_index = output_buffer->index();
  output_buffer->SetRange(0, input_entry.buffer->size());
  memcpy(output_buffer->data(), input_entry.buffer->data(),
         input_entry.buffer->size());

  // Release input buffer
  input_entry.state = BufferState::FREE;
  if (callback_) {
    callback_->OnInputBufferAvailable(input_index);
  }

  // Queue output buffer
  output_queue_.push(output_index);
  if (callback_) {
    callback_->OnOutputBufferAvailable(output_index);
  }
}

std::shared_ptr<CodecBuffer> DummyCodec::DequeueInputBuffer(
    int32_t index,
    int64_t timeout_ms) {
  return FindAvailableInputBuffer(index);
}

status_t DummyCodec::QueueInputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                                      int64_t timeout_ms) {
  size_t index = buffer->index();
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (!started_ || index >= input_buffers_.size() ||
        !IsBufferInUse(input_buffers_[index])) {
      return INVALID_OPERATION;
    }
  }

  // Schedule processing
  task_runner_->PostTask([this, index]() { ProcessBuffer(index); });
  return OK;
}

std::shared_ptr<CodecBuffer> DummyCodec::DequeueOutputBuffer(
    int32_t index,
    int64_t timeout_ms) {
  std::unique_lock<std::mutex> lock(lock_);
  if (!started_) {
    return nullptr;
  }

  if (output_queue_.empty()) {
    return nullptr;
  }

  size_t out_index = output_queue_.front();
  if (index >= 0 && static_cast<size_t>(index) != out_index) {
    return nullptr;
  }

  output_queue_.pop();
  return output_buffers_[out_index].buffer;
}

status_t DummyCodec::ReleaseOutputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                                         bool render) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_) {
    return INVALID_OPERATION;
  }

  size_t index = buffer->index();
  if (index >= output_buffers_.size() ||
      !IsBufferInUse(output_buffers_[index])) {
    return INVALID_OPERATION;
  }

  output_buffers_[index].state = BufferState::FREE;
  cv_.notify_all();
  return OK;
}

status_t DummyCodec::Stop() {
  std::lock_guard<std::mutex> lock(lock_);
  started_ = false;
  return OK;
}

status_t DummyCodec::Reset() {
  std::lock_guard<std::mutex> lock(lock_);
  while (!output_queue_.empty()) {
    output_queue_.pop();
  }

  for (auto& entry : input_buffers_) {
    entry.state = BufferState::FREE;
  }
  for (auto& entry : output_buffers_) {
    entry.state = BufferState::FREE;
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
  return OK;
}

}  // namespace test
}  // namespace media
}  // namespace ave
