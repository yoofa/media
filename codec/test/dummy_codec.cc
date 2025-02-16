/*
 * dummy_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "dummy_codec.h"

#include "base/logging.h"

namespace ave {
namespace media {
namespace test {

namespace {
const int kMaxInputBuffers = 8;
const int kMaxOutputBuffers = 8;
}  // namespace

DummyCodec::DummyCodec(bool is_encoder) : is_encoder_(is_encoder) {
  input_buffers_ = std::vector<BufferEntry>(kMaxInputBuffers);
  for (auto& entry : input_buffers_) {
    entry.buffer = std::make_shared<CodecBuffer>(1024 * 1024);  // 1MB buffer
  }

  output_buffers_ = std::vector<BufferEntry>(kMaxOutputBuffers);
  for (auto& entry : output_buffers_) {
    entry.buffer = std::make_shared<CodecBuffer>(1024 * 1024);  // 1MB buffer
  }
}

DummyCodec::~DummyCodec() {
  Stop();
  Release();
}

status_t DummyCodec::Configure(const std::shared_ptr<CodecConfig>& config) {
  std::lock_guard<std::mutex> lock(lock_);
  config_ = config;
  return OK;
}

status_t DummyCodec::SetCallback(CodecCallback* callback) {
  std::lock_guard<std::mutex> lock(lock_);
  callback_ = callback;
  return OK;
}

status_t DummyCodec::Start() {
  std::lock_guard<std::mutex> lock(lock_);
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

std::shared_ptr<CodecBuffer> DummyCodec::DequeueInputBuffer(
    int32_t index,
    int64_t timeout_ms) {
  std::unique_lock<std::mutex> lock(lock_);
  if (!started_) {
    return nullptr;
  }

  auto find_available = [this, index]() -> std::shared_ptr<CodecBuffer> {
    if (index < 0) {
      for (size_t i = 0; i < input_buffers_.size(); i++) {
        if (!input_buffers_[i].in_use) {
          input_buffers_[i].in_use = true;
          input_buffers_[i].buffer->SetIndex(i);
          return input_buffers_[i].buffer;
        }
      }
    } else if (static_cast<size_t>(index) < input_buffers_.size() &&
               !input_buffers_[index].in_use) {
      input_buffers_[index].in_use = true;
      input_buffers_[index].buffer->SetIndex(index);
      return input_buffers_[index].buffer;
    }
    return nullptr;
  };

  auto buffer = find_available();
  if (buffer || timeout_ms == 0) {
    return buffer;
  }

  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < end) {
    if (cv_.wait_until(lock, end, [&buffer, &find_available]() {
          buffer = find_available();
          return buffer != nullptr;
        })) {
      break;
    }
  }
  return buffer;
}

status_t DummyCodec::QueueInputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                                      int64_t timeout_ms) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_) {
    return INVALID_OPERATION;
  }

  size_t index = buffer->index();
  if (index >= input_buffers_.size() || !input_buffers_[index].in_use) {
    return INVALID_OPERATION;
  }

  // For dummy codec, we simply copy input to output
  for (size_t i = 0; i < output_buffers_.size(); i++) {
    if (!output_buffers_[i].in_use) {
      output_buffers_[i].in_use = true;
      output_buffers_[i].buffer->SetRange(0, buffer->size());
      memcpy(output_buffers_[i].buffer->data(), buffer->data(), buffer->size());
      output_queue_.push(i);
      if (callback_) {
        callback_->OnOutputBufferAvailable(i);
      }
      break;
    }
  }

  input_buffers_[index].in_use = false;
  if (callback_) {
    callback_->OnInputBufferAvailable(index);
  }

  cv_.notify_all();
  return OK;
}

std::shared_ptr<CodecBuffer> DummyCodec::DequeueOutputBuffer(
    int32_t index,
    int64_t timeout_ms) {
  std::unique_lock<std::mutex> lock(lock_);
  if (!started_) {
    return nullptr;
  }

  auto get_output = [this, index]() -> std::shared_ptr<CodecBuffer> {
    if (!output_queue_.empty()) {
      size_t front_index = output_queue_.front();
      if (index < 0 || static_cast<size_t>(index) == front_index) {
        output_queue_.pop();
        return output_buffers_[front_index].buffer;
      }
    }
    return nullptr;
  };

  auto buffer = get_output();
  if (buffer || timeout_ms == 0) {
    return buffer;
  }

  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < end) {
    if (cv_.wait_until(lock, end, [&buffer, &get_output]() {
          buffer = get_output();
          return buffer != nullptr;
        })) {
      break;
    }
  }
  return buffer;
}

status_t DummyCodec::ReleaseOutputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                                         bool render) {
  std::lock_guard<std::mutex> lock(lock_);
  if (!started_) {
    return INVALID_OPERATION;
  }

  size_t index = buffer->index();
  if (index >= output_buffers_.size() || !output_buffers_[index].in_use) {
    return INVALID_OPERATION;
  }

  output_buffers_[index].in_use = false;
  cv_.notify_all();
  return OK;
}

}  // namespace test
}  // namespace media
}  // namespace ave
