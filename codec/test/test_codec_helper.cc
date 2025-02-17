/*
 * test_codec_helper.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "test_codec_helper.h"

#include "base/errors.h"
#include "base/logging.h"
#include "base/task_util/task_runner_factory.h"
#include "media/codec/codec_factory.h"
#include "media/foundation/media_errors.h"

namespace ave {
namespace media {
namespace test {

CodecPlatform NameToCodecPlatform(const std::string& name) {
  CodecPlatform platform = CodecPlatform::kDummy;
  if (name == "ffmpeg") {
    platform = kFFmpeg;
  } else if (name == "android_ndk") {
    platform = kAndroidNdkMediaCodec;
  } else if (name == "android_java") {
    platform = kAndroidJavaMediaCodec;
  }
  return platform;
}

TestCodecRunner::TestCodecRunner(CodecFactory* factory,
                                 base::TaskRunnerFactory* task_runner_factory,
                                 InputDataCallback input_cb,
                                 OutputDataCallback output_cb)
    : factory_(factory),
      input_cb_(std::move(input_cb)),
      output_cb_(std::move(output_cb)),
      is_encoder_(false),
      task_runner_(std::make_unique<base::TaskRunner>(
          task_runner_factory->CreateTaskRunner(
              "TestCodecRunner",
              base::TaskRunnerFactory::Priority::NORMAL))) {}

TestCodecRunner::~TestCodecRunner() {
  Stop();
}

status_t TestCodecRunner::Init(const std::string& mime,
                               bool is_encoder,
                               const std::shared_ptr<MediaFormat>& format) {
  is_encoder_ = is_encoder;
  format_ = format;

  codec_ = factory_->CreateCodecByMime(mime, is_encoder);
  if (!codec_) {
    AVE_LOG(LS_ERROR) << "Failed to create codec for mime: " << mime;
    return ERROR_UNSUPPORTED;
  }

  auto config = std::make_shared<CodecConfig>();
  config->format = format;

  status_t err = codec_->Configure(config);
  if (err != OK) {
    AVE_LOG(LS_ERROR) << "Failed to configure codec: " << err;
    return err;
  }

  err = codec_->SetCallback(this);  // Set this as the callback
  if (err != OK) {
    AVE_LOG(LS_ERROR) << "Failed to set callback: " << err;
    return err;
  }

  return OK;
}

status_t TestCodecRunner::Start() {
  if (running_) {
    return INVALID_OPERATION;
  }

  status_t err = codec_->Start();
  if (err != OK) {
    AVE_LOG(LS_ERROR) << "Failed to start codec: " << err;
    return err;
  }

  running_ = true;
  error_ = false;
  eos_sent_ = false;

  return OK;
}

status_t TestCodecRunner::Stop() {
  if (!running_) {
    return OK;
  }

  running_ = false;
  if (codec_) {
    codec_->Stop();
    codec_->Release();
    codec_ = nullptr;
  }

  // Signal completion in case someone is waiting
  {
    std::lock_guard<std::mutex> lock(completion_lock_);
    completed_ = true;
    completion_cv_.notify_all();
  }

  return OK;
}

// CodecCallback implementation
void TestCodecRunner::OnInputBufferAvailable(size_t index) {
  AVE_LOG(LS_INFO) << "Input buffer available: " << index;
  if (!running_ || error_) {
    return;
  }

  task_runner_->PostTask([this, index]() { ProcessInput(index); });
}

void TestCodecRunner::OnOutputBufferAvailable(size_t index) {
  AVE_LOG(LS_INFO) << "Output buffer available: " << index;
  if (!running_ || error_) {
    return;
  }

  task_runner_->PostTask([this, index]() { ProcessOutput(index); });
}

void TestCodecRunner::OnOutputFormatChanged(
    const std::shared_ptr<Message>& format) {
  AVE_LOG(LS_INFO) << "Output format changed";
}

void TestCodecRunner::OnError(status_t error) {
  AVE_LOG(LS_ERROR) << "Codec error: " << error;
  task_runner_->PostTask([this, error]() { HandleError(error); });
}

void TestCodecRunner::OnFrameRendered(std::shared_ptr<Message> notify) {
  AVE_LOG(LS_INFO) << "Frame rendered";
}

void TestCodecRunner::ProcessInput(size_t index) {
  if (!running_ || error_ || eos_sent_) {
    return;
  }

  auto input_buffer = codec_->DequeueInputBuffer(index, 0);
  if (!input_buffer) {
    return;
  }

  const size_t kBufferSize = 4096;
  std::vector<uint8_t> buffer(kBufferSize);

  ssize_t bytes_read = input_cb_(buffer.data(), kBufferSize);
  if (bytes_read < 0) {
    HandleError(bytes_read);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(completion_lock_);
    has_pending_input_ = true;
  }

  if (bytes_read == 0) {
    // End of stream
    input_buffer->SetRange(0, 0);
    codec_->QueueInputBuffer(input_buffer, -1);
    eos_sent_ = true;

    std::lock_guard<std::mutex> lock(completion_lock_);
    has_pending_input_ = false;
    CheckCompletion();
    return;
  }

  input_buffer->SetRange(0, bytes_read);
  memcpy(input_buffer->data(), buffer.data(), bytes_read);
  status_t err = codec_->QueueInputBuffer(input_buffer, -1);
  if (err != OK) {
    AVE_LOG(LS_ERROR) << "Failed to queue input buffer: " << err;
    HandleError(err);
  }
}

void TestCodecRunner::ProcessOutput(size_t index) {
  if (!running_ || error_) {
    return;
  }

  auto output_buffer = codec_->DequeueOutputBuffer(index, 0);
  if (!output_buffer) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(completion_lock_);
    has_pending_output_ = true;
  }

  ssize_t bytes_written =
      output_cb_(output_buffer->data(), output_buffer->size());
  if (bytes_written < 0) {
    HandleError(bytes_written);
    return;
  }

  status_t err = codec_->ReleaseOutputBuffer(output_buffer, false);
  if (err != OK) {
    HandleError(err);
  }

  {
    std::lock_guard<std::mutex> lock(completion_lock_);
    has_pending_output_ = false;
    CheckCompletion();
  }
}

void TestCodecRunner::CheckCompletion() {
  // Must be called with completion_lock_ held
  if (eos_sent_ && !has_pending_input_ && !has_pending_output_) {
    completed_ = true;
    completion_cv_.notify_all();
  }
}

status_t TestCodecRunner::WaitForCompletion() {
  std::unique_lock<std::mutex> lock(completion_lock_);
  completion_cv_.wait(lock, [this]() { return completed_ || error_; });
  return error_ ? ERROR_IO : OK;
}

void TestCodecRunner::HandleError(status_t error) {
  AVE_LOG(LS_ERROR) << "TestCodecRunner error: " << error;
  error_ = true;
  Stop();
}

}  // namespace test
}  // namespace media
}  // namespace ave
