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

#include "../../modules/ffmpeg/ffmpeg_utils.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
}

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
      ffmpeg_utils::ConfigureVideoCodec(format.get(), codec_ctx_);
    } else if (format->stream_type() == MediaType::AUDIO) {
      ffmpeg_utils::ConfigureAudioCodec(format.get(), codec_ctx_);
    }

    // TODO: use buffer cnt in config
    input_buffers_ = std::vector<BufferEntry>(kMaxInputBuffers);
    for (size_t i = 0; i < input_buffers_.size(); ++i) {
      input_buffers_[i].buffer = std::make_shared<CodecBuffer>(1);
    }
    output_buffers_ = std::vector<BufferEntry>(kMaxOutputBuffers);
    for (size_t i = 0; i < output_buffers_.size(); ++i) {
      output_buffers_[i].buffer = std::make_shared<CodecBuffer>(1);
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

status_t FFmpegCodec::GetInputBuffer(size_t index,
                                     std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  buffer = input_buffers_[index].buffer;
  return OK;
}
status_t FFmpegCodec::GetOutputBuffer(size_t index,
                                      std::shared_ptr<CodecBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(lock_);
  buffer = output_buffers_[index].buffer;
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

ssize_t FFmpegCodec::DequeueInputBuffer(int64_t timeout_ms) {
  auto dequeue_index = [this]() -> ssize_t {
    std::lock_guard<std::mutex> lock(lock_);
    for (size_t i = 0; i < input_buffers_.size(); ++i) {
      if (!input_buffers_[i].in_use) {
        input_buffers_[i].in_use = true;
        return static_cast<ssize_t>(i);
      }
    }
    return -1;
  };

  ssize_t index = dequeue_index();
  if (index >= 0 || timeout_ms == 0) {
    return index;
  }

  std::unique_lock<std::mutex> lock(lock_);
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  cv_.wait_until(lock, end, [&index, &dequeue_index]() {
    index = dequeue_index();
    return index >= 0;
  });

  return index;
}

status_t FFmpegCodec::QueueInputBuffer(size_t index) {
  std::lock_guard<std::mutex> lock(lock_);
  if (index >= input_buffers_.size() || !input_buffers_[index].in_use) {
    return INVALID_OPERATION;
  }

  input_queue_.push(index);

  // Trigger processing on task runner
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  return OK;
}

ssize_t FFmpegCodec::DequeueOutputBuffer(int64_t timeout_ms) {
  auto dequeue_index = [this]() -> ssize_t {
    std::lock_guard<std::mutex> lock(lock_);
    if (!output_queue_.empty()) {
      size_t front_index = output_queue_.front();
      output_queue_.pop();
      return static_cast<ssize_t>(front_index);
    }
    return -1;
  };

  ssize_t index = dequeue_index();
  if (index >= 0 || timeout_ms == 0) {
    return index;
  }

  std::unique_lock<std::mutex> lock(lock_);
  task_runner_->PostTask([this]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    Process();
  });

  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  cv_.wait_until(lock, end, [&index, &dequeue_index]() {
    index = dequeue_index();
    return index >= 0;
  });

  return index;
}

status_t FFmpegCodec::ReleaseOutputBuffer(size_t index, bool render) {
  status_t ret = OK;
  task_runner_->PostTaskAndWait([this, &ret, index, render]() {
    AVE_DCHECK_RUN_ON(task_runner_.get());
    std::lock_guard<std::mutex> lock(lock_);
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

  // Copy frame data to buffer
  auto& buffer = output_buffers_[index].buffer;

  if (codec_ctx_->codec_type == AVMEDIA_TYPE_AUDIO) {
    // Audio: copy PCM data
    int data_size = av_samples_get_buffer_size(
        nullptr, frame->ch_layout.nb_channels, frame->nb_samples,
        static_cast<AVSampleFormat>(frame->format), 1);

    if (data_size > 0) {
      buffer->EnsureCapacity(data_size, false);

      // For planar audio, need to interleave
      if (av_sample_fmt_is_planar(static_cast<AVSampleFormat>(frame->format))) {
        int channels = frame->ch_layout.nb_channels;
        int samples = frame->nb_samples;
        int bytes_per_sample =
            av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));

        uint8_t* dst = buffer->data();
        for (int sample = 0; sample < samples; sample++) {
          for (int ch = 0; ch < channels; ch++) {
            std::memcpy(dst, frame->data[ch] + sample * bytes_per_sample,
                        bytes_per_sample);
            dst += bytes_per_sample;
          }
        }
      } else {
        // Packed audio, direct copy
        std::memcpy(buffer->data(), frame->data[0], data_size);
      }
      buffer->SetRange(0, data_size);
    }
  } else if (codec_ctx_->codec_type == AVMEDIA_TYPE_VIDEO) {
    // Video: copy YUV data
    int data_size =
        av_image_get_buffer_size(static_cast<AVPixelFormat>(frame->format),
                                 frame->width, frame->height, 1);

    if (data_size > 0) {
      buffer->EnsureCapacity(data_size, false);
      av_image_copy_to_buffer(
          buffer->data(), data_size, const_cast<const uint8_t**>(frame->data),
          frame->linesize, static_cast<AVPixelFormat>(frame->format),
          frame->width, frame->height, 1);
      buffer->SetRange(0, data_size);
    }
  }

  output_buffers_[index].in_use = true;
  output_queue_.push(index);
  OnOutputBufferAvailable(index);

  av_frame_free(&frame);
}

void FFmpegCodec::Process() {
  MaybeSendPacket();
  MaybeReceiveFrame();

  // Don't schedule next process here - let it be triggered by input/output
  // events This avoids blocking PostTaskAndWait calls
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
