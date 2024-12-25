/*
 * ffmpeg_codec.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "ffmpeg_codec.h"

#include "base/attributes.h"
#include "base/sequence_checker.h"
#include "base/task_util/default_task_runner_factory.h"

#include "ffmpeg_codec_utils.h"

namespace ave {
namespace media {

namespace {
const int kMaxInputBuffers = 8;
const int kMaxOutputBuffers = 7;
}  // namespace

FFmpegCodec::FFmpegCodec(const AVCodec* codec, bool is_encoder)
    : codec_(codec),
      is_encoder_(is_encoder),
      task_runner_(std::make_unique<base::TaskRunner>(
          base::CreateDefaultTaskRunnerFactory()->CreateTaskRunner(
              "FFmpegCodec",
              base::TaskRunnerFactory::Priority::NORMAL))),
      codec_ctx_(nullptr),
      callback_(nullptr) {}

FFmpegCodec::~FFmpegCodec() {
  Release();
}

status_t FFmpegCodec::Configure(const std::shared_ptr<CodecConfig>& config) {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret, config]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    std::lock_guard<std::mutex> lock(lock_);

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
      ret = NO_MEMORY;
      return;
    }

    // Configure codec context based on CodecConfig
    // This is a simplified example, you'll need to map CodecConfig to
    // AVCodecContext fields codec_ctx_->width = config->info.width;
    // codec_ctx_->height = config->info.height;
    // TODO: config to codec_ctx_ mapping
    auto format = config->format;
    if (format->stream_type() == MediaType::VIDEO) {
      ConfigureVideoCodec(format.get(), codec_ctx_);
    } else if (format->stream_type() == MediaType::AUDIO) {
      ConfigureAudioCodec(format.get(), codec_ctx_);
    }

    // TODO: use buffer cnt in config
    input_buffers_ = std::vector<BufferEntry>(kMaxInputBuffers);
    for (auto& entry : input_buffers_) {
      entry.buffer = std::make_shared<CodecBuffer>(1);
    }
    output_buffers_ = std::vector<BufferEntry>(kMaxOutputBuffers);
    for (auto& entry : output_buffers_) {
      entry.buffer = std::make_shared<CodecBuffer>(1);
    }

    if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) {
      avcodec_free_context(&codec_ctx_);
      ret = UNKNOWN_ERROR;
      return;
    }

    return;
  });

  return ret;
}

status_t FFmpegCodec::SetCallback(CodecCallback* callback) {
  task_runner_->PostTaskAndWait([this, callback]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    callback_ = callback;
  });
  return OK;
}

status_t FFmpegCodec::Start() {
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    //
    Process();
  });
  return OK;
}

status_t FFmpegCodec::Stop() {
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    //
    Process();
  });
  return OK;
}

status_t FFmpegCodec::Reset() {
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    //
    Process();
  });
  return OK;
}

status_t FFmpegCodec::Flush() {
  task_runner_->PostTaskAndWait([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    //
  });
  return OK;
}

status_t FFmpegCodec::Release() {
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    //
    Process();
  });
  return OK;
}

std::shared_ptr<CodecBuffer> FFmpegCodec::DequeueInputBuffer(
    int32_t index,
    int64_t timeout_ms) {
  auto dequeue_buffer = [this, index]() -> std::shared_ptr<CodecBuffer> {
    std::lock_guard<std::mutex> lock(lock_);
    if (index < 0) {
      for (auto& entry : input_buffers_) {
        if (!entry.in_use) {
          entry.in_use = true;
          return entry.buffer;
        }
      }
    } else if (static_cast<size_t>(index) < input_buffers_.size() &&
               !input_buffers_[index].in_use) {
      input_buffers_[index].in_use = true;
      return input_buffers_[index].buffer;
    }
    return nullptr;
  };

  auto buffer = dequeue_buffer();
  if (buffer || timeout_ms == 0) {
    return buffer;
  }

  std::unique_lock<std::mutex> lock(lock_);
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  cv_.wait_until(lock, end, [&buffer, &dequeue_buffer]() {
    buffer = dequeue_buffer();
    return buffer != nullptr;
  });

  return buffer;
}

status_t FFmpegCodec::QueueInputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                                       int64_t timeout_ms AVE_MAYBE_UNUSED) {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret, &buffer]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    std::lock_guard<std::mutex> lock(lock_);
    size_t index = buffer->index();
    if (index >= input_buffers_.size() ||
        (index >= 0 && !input_buffers_[index].in_use)) {
      ret = INVALID_OPERATION;
      return;
    }

    // TODO: index = -1, buffer is not from input_buffers_ in this codec, copy
    // it to an available input buffer

    input_queue_.push(index);
    Process();
    ret = OK;
  });
  return ret;
}

std::shared_ptr<CodecBuffer> FFmpegCodec::DequeueOutputBuffer(
    int32_t index,
    int64_t timeout_ms) {
  auto dequeue_buffer = [this, index]() -> std::shared_ptr<CodecBuffer> {
    std::lock_guard<std::mutex> lock(lock_);
    if (!output_queue_.empty()) {
      size_t front_index = output_queue_.front();
      if (index < 0 || static_cast<size_t>(index) == front_index) {
        output_queue_.pop();
        return output_buffers_[front_index].buffer;
      }
    }
    return nullptr;
  };

  auto buffer = dequeue_buffer();
  if (buffer || timeout_ms == 0) {
    return buffer;
  }

  std::unique_lock<std::mutex> lock(lock_);
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  cv_.wait_until(lock, end, [&buffer, &dequeue_buffer]() {
    buffer = dequeue_buffer();
    return buffer != nullptr;
  });

  return buffer;
}

status_t FFmpegCodec::ReleaseOutputBuffer(std::shared_ptr<CodecBuffer>& buffer,
                                          bool render) {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret, &buffer, render]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    std::lock_guard<std::mutex> lock(lock_);
    size_t index = buffer->index();
    if (index >= output_buffers_.size() || !output_buffers_[index].in_use) {
      ret = INVALID_OPERATION;
      return;
    }

    if (render) {
      // TODO: Implement rendering logic if supported
    }

    output_buffers_[index].in_use = false;
    Process();
    ret = OK;
  });
  return ret;
}

void FFmpegCodec::MaybeSendPacket() {
  std::lock_guard<std::mutex> lock(lock_);
  if (input_queue_.empty()) {
    // no input buffer, just return
    return;
  }

  auto index = input_queue_.front();

  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    OnError(NO_MEMORY);
    return;
  }

  auto& buffer = input_buffers_[index].buffer;
  pkt->data = buffer->data();
  pkt->size = static_cast<int>(buffer->size());
  // TODO: get format from CodecBuffer
  // pkt->pts = buffer->;
  auto ret = avcodec_send_packet(codec_ctx_, pkt);
  av_packet_free(&pkt);

  if (ret < 0) {
    OnError(UNKNOWN_ERROR);
    return;
  }
  input_buffers_[index].in_use = false;
  input_queue_.pop();

  cv_.notify_all();
  OnInputBufferAvailable(static_cast<int32_t>(index));
}

void FFmpegCodec::MaybeReceiveFrame() {
  std::lock_guard<std::mutex> lock(lock_);

  if (output_queue_.size() >= output_buffers_.size()) {
    // No available output buffers
    return;
  }

  size_t index = -1;

  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    if (!output_buffers_[i].in_use) {
      index = i;
      break;
    }
  }

  AVE_DCHECK(index >= 0);

  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    OnError(NO_MEMORY);
    return;
  }

  int ret = avcodec_receive_frame(codec_ctx_, frame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    av_frame_free(&frame);
    return;
  }

  if (ret < 0) {
    av_frame_free(&frame);
    OnError(UNKNOWN_ERROR);
    return;
  }

  // auto& buffer = output_buffers_[index].buffer;
  //  TODO: Copy frame data to buffer
  //  This will depend on the pixel format and other codec specifics
  //  buffer->setSize(...);
  //  buffer->setPresentationTimeUs(frame->pts);

  output_buffers_[index].in_use = true;
  output_queue_.push(index);
  OnOutputBufferAvailable(index);

  av_frame_free(&frame);
}

void FFmpegCodec::Process() {
  MaybeSendPacket();
  MaybeReceiveFrame();

  // run next process interval_ms later
  // calculate delay_ms later
  int delay_ms = 10;
  task_runner_->PostDelayedTask(
      [this]() {
        AVE_DCHECK_RUN_ON(task_runner_.get());
        Process();
      },
      delay_ms);
}

void FFmpegCodec::OnInputBufferAvailable(size_t index) {
  if (callback_) {
    callback_->OnInputBufferAvailable(index);
  }
}

void FFmpegCodec::OnOutputBufferAvailable(size_t index) {
  if (callback_) {
    callback_->OnOutputBufferAvailable(index);
  }
}

void FFmpegCodec::OnError(status_t error) {
  if (callback_) {
    callback_->OnError(error);
  }
}

}  // namespace media
}  // namespace ave
