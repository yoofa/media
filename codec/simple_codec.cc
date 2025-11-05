/*
 * simple_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "simple_codec.h"

#include "base/attributes.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task_util/default_task_runner_factory.h"

namespace ave {
namespace media {

namespace {
const int32_t kMaxInputBuffers = 8;
const int32_t kMaxOutputBuffers = 16;
const int32_t kDefaultBufferSize = 10 * 1024 * 1024;  // 4MB
const size_t kInvalidIndex = static_cast<size_t>(-1);
}  // namespace

SimpleCodec::SimpleCodec(bool is_encoder)
    : is_encoder_(is_encoder),
      task_runner_(std::make_unique<base::TaskRunner>(
          base::CreateDefaultTaskRunnerFactory()->CreateTaskRunner(
              "SimpleCodec",
              base::TaskRunnerFactory::Priority::NORMAL))),
      state_(State::UNINITIALIZED),
      callback_(nullptr) {}

SimpleCodec::~SimpleCodec() {
  Release();
}

status_t SimpleCodec::Configure(const std::shared_ptr<CodecConfig>& config) {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret, config]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());

    if (state_ != State::UNINITIALIZED) {
      ret = INVALID_OPERATION;
      return;
    }

    std::lock_guard<std::mutex> lock(lock_);

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

    config_ = config;
    ret = OnConfigure(config);
    if (ret == OK) {
      state_ = State::CONFIGURED;
    } else {
      state_ = State::ERROR;
    }
  });

  return ret;
}

status_t SimpleCodec::SetCallback(CodecCallback* callback) {
  task_runner_->PostTaskAndWait([this, callback]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    callback_ = callback;
  });
  return OK;
}

status_t SimpleCodec::Start() {
  status_t ret = OK;
  task_runner_->PostTask([this, &ret]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());

    if (state_ != State::CONFIGURED && state_ != State::STOPPED) {
      ret = INVALID_OPERATION;
      NotifyError(ret);
      return;
    }

    ret = OnStart();
    if (ret == OK) {
      state_ = State::STARTED;
      Process();
    } else {
      state_ = State::ERROR;
      NotifyError(ret);
    }
  });
  return OK;
}

status_t SimpleCodec::Stop() {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());

    if (state_ != State::STARTED) {
      ret = INVALID_OPERATION;
      return;
    }

    ret = OnStop();
    if (ret == OK) {
      state_ = State::STOPPED;
    } else {
      state_ = State::ERROR;
      NotifyError(ret);
    }
  });
  return ret;
}

status_t SimpleCodec::Reset() {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    std::lock_guard<std::mutex> lock(lock_);

    ret = OnReset();
    if (ret == OK) {
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
      state_ = State::CONFIGURED;
    } else {
      state_ = State::ERROR;
      NotifyError(ret);
    }
  });
  return ret;
}

status_t SimpleCodec::Flush() {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    std::lock_guard<std::mutex> lock(lock_);

    if (state_ != State::STARTED) {
      ret = INVALID_OPERATION;
      return;
    }

    ret = OnFlush();
    if (ret != OK) {
      state_ = State::ERROR;
      NotifyError(ret);
    }
  });
  return ret;
}

status_t SimpleCodec::Release() {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());

    if (state_ == State::RELEASED) {
      return;
    }

    ret = OnRelease();
    state_ = State::RELEASED;
    callback_ = nullptr;

    std::lock_guard<std::mutex> lock(lock_);
    input_buffers_.clear();
    output_buffers_.clear();
    while (!input_queue_.empty()) {
      input_queue_.pop();
    }
    while (!output_queue_.empty()) {
      output_queue_.pop();
    }
  });
  return ret;
}

status_t SimpleCodec::GetInputBuffer(size_t index,
                                     std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  if (index >= input_buffers_.size()) {
    return INVALID_OPERATION;
  }
  buffer = input_buffers_[index].buffer;
  return OK;
}

status_t SimpleCodec::GetOutputBuffer(size_t index,
                                      std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  if (index >= output_buffers_.size()) {
    return INVALID_OPERATION;
  }
  buffer = output_buffers_[index].buffer;
  return OK;
}

ssize_t SimpleCodec::DequeueInputBuffer(int64_t timeout_ms)
    NO_THREAD_SAFETY_ANALYSIS {
  {
    std::lock_guard<std::mutex> lock(lock_);
    for (size_t i = 0; i < input_buffers_.size(); ++i) {
      if (!input_buffers_[i].in_use) {
        input_buffers_[i].in_use = true;
        return static_cast<ssize_t>(i);
      }
    }
  }

  if (timeout_ms == 0) {
    return -1;
  }

  // Post task to process
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  // Wait for input buffer with lock
  std::unique_lock<std::mutex> lock(lock_);
  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
  cv_.wait_until(lock, end, [this]() {
    for (const auto& entry : input_buffers_) {
      if (!entry.in_use) {
        return true;
      }
    }
    return false;
  });

  // Check again after wait
  for (size_t i = 0; i < input_buffers_.size(); ++i) {
    if (!input_buffers_[i].in_use) {
      input_buffers_[i].in_use = true;
      return static_cast<ssize_t>(i);
    }
  }
#pragma clang diagnostic pop

  return -1;
}

status_t SimpleCodec::QueueInputBuffer(size_t index) {
  std::lock_guard<std::mutex> lock(lock_);
  if (index >= input_buffers_.size() || !input_buffers_[index].in_use) {
    return INVALID_OPERATION;
  }

  input_queue_.push(index);

  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  return OK;
}

ssize_t SimpleCodec::DequeueOutputBuffer(int64_t timeout_ms)
    NO_THREAD_SAFETY_ANALYSIS {
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (!output_queue_.empty()) {
      size_t front_index = output_queue_.front();
      output_queue_.pop();
      return static_cast<ssize_t>(front_index);
    }
  }

  if (timeout_ms == 0) {
    return -1;
  }

  // Post task to process
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  // Wait for output with lock
  std::unique_lock<std::mutex> lock(lock_);
  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
  cv_.wait_until(lock, end, [this]() { return !output_queue_.empty(); });

  // Check again after wait
  if (!output_queue_.empty()) {
    size_t front_index = output_queue_.front();
    output_queue_.pop();
    return static_cast<ssize_t>(front_index);
  }
#pragma clang diagnostic pop

  return -1;
}

status_t SimpleCodec::ReleaseOutputBuffer(size_t index, bool render) {
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (index >= output_buffers_.size() || !output_buffers_[index].in_use) {
      return INVALID_OPERATION;
    }

    if (render) {
      // TODO: Implement rendering logic if supported
    }

    output_buffers_[index].in_use = false;
    cv_.notify_all();
  }

  // Post Process task asynchronously to avoid blocking
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  return OK;
}

void SimpleCodec::Process() {
  if (state_ != State::STARTED) {
    AVE_LOG(LS_WARNING) << "Process called but state is not STARTED";
    return;
  }

  AVE_LOG(LS_VERBOSE) << "SimpleCodec::Process() called";

  size_t input_index = kInvalidIndex;
  {
    std::lock_guard<std::mutex> lock(lock_);
    AVE_LOG(LS_VERBOSE) << "Input queue size: " << input_queue_.size();
    if (!input_queue_.empty()) {
      input_index = input_queue_.front();
      input_queue_.pop();
      AVE_LOG(LS_INFO) << "Will process input buffer index: " << input_index;
    } else {
      AVE_LOG(LS_VERBOSE) << "Input queue is empty";
    }
  }

  // Process input without holding lock
  if (input_index != kInvalidIndex) {
    AVE_LOG(LS_VERBOSE) << "Calling ProcessInput(" << input_index << ")";
    ProcessInput(input_index);
    AVE_LOG(LS_VERBOSE) << "ProcessInput completed";
  }

  // Process all available outputs
  AVE_LOG(LS_VERBOSE) << "Starting output processing loop";
  while (state_ == State::STARTED) {
    size_t output_count_before{0};
    {
      std::lock_guard<std::mutex> lock(lock_);
      output_count_before = output_queue_.size();
    }

    ProcessOutput();

    size_t output_count_after{0};
    {
      std::lock_guard<std::mutex> lock(lock_);
      output_count_after = output_queue_.size();
    }

    // If no new output was produced, stop trying
    if (output_count_after <= output_count_before) {
      break;
    }
  }
}

void SimpleCodec::NotifyInputBufferAvailable(size_t index) {
  if (callback_) {
    callback_->OnInputBufferAvailable(index);
  }
}

void SimpleCodec::NotifyOutputBufferAvailable(size_t index) {
  if (callback_) {
    callback_->OnOutputBufferAvailable(index);
  }
}

void SimpleCodec::NotifyOutputFormatChanged(
    const std::shared_ptr<MediaMeta>& format) {
  if (callback_) {
    callback_->OnOutputFormatChanged(format);
  }
}

void SimpleCodec::NotifyError(status_t error) {
  if (callback_) {
    callback_->OnError(error);
  }
}

size_t SimpleCodec::GetAvailableOutputBufferIndex() {
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    if (!output_buffers_[i].in_use) {
      return i;
    }
  }
  return kInvalidIndex;
}

size_t SimpleCodec::PushOutputBuffer(size_t index) {
  if (index < output_buffers_.size()) {
    output_buffers_[index].in_use = true;
    output_queue_.push(index);
    cv_.notify_all();
    return index;
  }
  return kInvalidIndex;
}

}  // namespace media
}  // namespace ave
