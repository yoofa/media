/*
 * ffmpeg_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "ffmpeg_codec.h"

#include "base/attributes.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "media/modules/ffmpeg/ffmpeg_utils.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
}

namespace ave {
namespace media {

namespace {
const size_t kInvalidIndex = static_cast<size_t>(-1);
}  // namespace

FFmpegCodec::FFmpegCodec(const AVCodec* codec, bool is_encoder)
    : SimpleCodec(is_encoder), codec_(codec), codec_ctx_(nullptr) {}

FFmpegCodec::~FFmpegCodec() {
  OnRelease();
}

status_t FFmpegCodec::OnConfigure(const std::shared_ptr<CodecConfig>& config) {
  codec_ctx_ = avcodec_alloc_context3(codec_);
  if (!codec_ctx_) {
    return NO_MEMORY;
  }

  auto format = config->format;
  if (format->stream_type() == MediaType::VIDEO) {
    ffmpeg_utils::ConfigureVideoCodec(format.get(), codec_ctx_);
  } else if (format->stream_type() == MediaType::AUDIO) {
    ffmpeg_utils::ConfigureAudioCodec(format.get(), codec_ctx_);
  }

  if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) {
    avcodec_free_context(&codec_ctx_);
    return UNKNOWN_ERROR;
  }

  return OK;
}

status_t FFmpegCodec::OnStart() {
  return OK;
}

status_t FFmpegCodec::OnStop() {
  return OK;
}

status_t FFmpegCodec::OnReset() {
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_);
  }
  return OK;
}

status_t FFmpegCodec::OnFlush() {
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_);
  }
  return OK;
}

status_t FFmpegCodec::OnRelease() {
  if (codec_ctx_) {
    avcodec_free_context(&codec_ctx_);
    codec_ctx_ = nullptr;
  }
  return OK;
}

void FFmpegCodec::ProcessInput(size_t index) {
  AVE_LOG(LS_INFO) << "FFmpegCodec::ProcessInput(" << index << ") called";
  std::lock_guard<std::mutex> lock(lock_);
  AVE_LOG(LS_INFO) << "FFmpegCodec::ProcessInput got lock";

  if (index >= input_buffers_.size() || !input_buffers_[index].in_use) {
    AVE_LOG(LS_WARNING) << "Invalid input buffer index or not in use";
    return;
  }

  auto& buffer = input_buffers_[index].buffer;
  AVE_LOG(LS_INFO) << "Input buffer size: " << buffer->size();

  if (is_encoder_) {
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
      NotifyError(NO_MEMORY);
      return;
    }

    // Configure frame based on codec type
    if (codec_ctx_->codec_type == AVMEDIA_TYPE_AUDIO) {
      frame->format = codec_ctx_->sample_fmt;
      frame->sample_rate = codec_ctx_->sample_rate;
      av_channel_layout_copy(&frame->ch_layout, &codec_ctx_->ch_layout);
      frame->nb_samples = codec_ctx_->frame_size;
    } else if (codec_ctx_->codec_type == AVMEDIA_TYPE_VIDEO) {
      frame->format = codec_ctx_->pix_fmt;
      frame->width = codec_ctx_->width;
      frame->height = codec_ctx_->height;
    }

    if (av_frame_get_buffer(frame, 0) < 0) {
      av_frame_free(&frame);
      NotifyError(NO_MEMORY);
      return;
    }

    // Copy buffer data to frame
    if (codec_ctx_->codec_type == AVMEDIA_TYPE_AUDIO) {
      size_t data_size = buffer->size();
      if (data_size > 0) {
        if (av_sample_fmt_is_planar(codec_ctx_->sample_fmt)) {
          int channels = frame->ch_layout.nb_channels;
          int samples = frame->nb_samples;
          int bytes_per_sample =
              av_get_bytes_per_sample(codec_ctx_->sample_fmt);
          const uint8_t* src = buffer->data();

          for (int sample = 0; sample < samples; sample++) {
            for (int ch = 0; ch < channels; ch++) {
              std::memcpy(frame->data[ch] + sample * bytes_per_sample, src,
                          bytes_per_sample);
              src += bytes_per_sample;
            }
          }
        } else {
          std::memcpy(frame->data[0], buffer->data(), data_size);
        }
      }
    } else if (codec_ctx_->codec_type == AVMEDIA_TYPE_VIDEO) {
      av_image_fill_arrays(frame->data, frame->linesize, buffer->data(),
                           codec_ctx_->pix_fmt, codec_ctx_->width,
                           codec_ctx_->height, 1);
    }

    int ret = avcodec_send_frame(codec_ctx_, frame);
    av_frame_free(&frame);

    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      NotifyError(UNKNOWN_ERROR);
      return;
    }
  } else {
    // Decoder: send packet
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
      NotifyError(NO_MEMORY);
      return;
    }

    pkt->data = buffer->data();
    pkt->size = static_cast<int>(buffer->size());

    int ret = avcodec_send_packet(codec_ctx_, pkt);
    av_packet_free(&pkt);

    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      NotifyError(UNKNOWN_ERROR);
      return;
    }
  }

  input_buffers_[index].in_use = false;
  cv_.notify_all();
  NotifyInputBufferAvailable(index);
}

void FFmpegCodec::ProcessOutput() {
  size_t pushed_index = kInvalidIndex;

  {
    std::lock_guard<std::mutex> lock(lock_);

    size_t index = GetAvailableOutputBufferIndex();
    if (index == static_cast<size_t>(-1)) {
      AVE_LOG(LS_VERBOSE) << "No available output buffer";
      return;
    }

    AVE_LOG(LS_VERBOSE) << "Processing output, buffer index: " << index;

    if (is_encoder_) {
      // Encoder: receive packet
      AVPacket* pkt = av_packet_alloc();
      if (!pkt) {
        NotifyError(NO_MEMORY);
        return;
      }

      int ret = avcodec_receive_packet(codec_ctx_, pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_packet_free(&pkt);
        return;
      }

      if (ret < 0) {
        av_packet_free(&pkt);
        NotifyError(UNKNOWN_ERROR);
        return;
      }

      auto& buffer = output_buffers_[index].buffer;
      buffer->EnsureCapacity(pkt->size, false);
      std::memcpy(buffer->data(), pkt->data, pkt->size);
      buffer->SetRange(0, pkt->size);

      av_packet_free(&pkt);
      pushed_index = PushOutputBuffer(index);
    } else {
      // Decoder: receive frame
      AVFrame* frame = av_frame_alloc();
      if (!frame) {
        NotifyError(NO_MEMORY);
        return;
      }

      int ret = avcodec_receive_frame(codec_ctx_, frame);
      if (ret == AVERROR(EAGAIN)) {
        AVE_LOG(LS_VERBOSE) << "Decoder needs more input (EAGAIN)";
        av_frame_free(&frame);
        return;
      }

      if (ret == AVERROR_EOF) {
        AVE_LOG(LS_INFO) << "Decoder reached EOF";
        av_frame_free(&frame);
        return;
      }

      if (ret < 0) {
        AVE_LOG(LS_ERROR) << "Decoder error: " << ret;
        av_frame_free(&frame);
        NotifyError(UNKNOWN_ERROR);
        return;
      }

      AVE_LOG(LS_INFO) << "Decoder produced output frame";

      auto& buffer = output_buffers_[index].buffer;

      if (codec_ctx_->codec_type == AVMEDIA_TYPE_AUDIO) {
        int data_size = av_samples_get_buffer_size(
            nullptr, frame->ch_layout.nb_channels, frame->nb_samples,
            static_cast<AVSampleFormat>(frame->format), 1);

        if (data_size > 0) {
          buffer->EnsureCapacity(data_size, false);

          if (av_sample_fmt_is_planar(
                  static_cast<AVSampleFormat>(frame->format))) {
            int channels = frame->ch_layout.nb_channels;
            int samples = frame->nb_samples;
            int bytes_per_sample = av_get_bytes_per_sample(
                static_cast<AVSampleFormat>(frame->format));

            uint8_t* dst = buffer->data();
            for (int sample = 0; sample < samples; sample++) {
              for (int ch = 0; ch < channels; ch++) {
                std::memcpy(dst, frame->data[ch] + sample * bytes_per_sample,
                            bytes_per_sample);
                dst += bytes_per_sample;
              }
            }
          } else {
            std::memcpy(buffer->data(), frame->data[0], data_size);
          }
          buffer->SetRange(0, data_size);
        }
      } else if (codec_ctx_->codec_type == AVMEDIA_TYPE_VIDEO) {
        int data_size =
            av_image_get_buffer_size(static_cast<AVPixelFormat>(frame->format),
                                     frame->width, frame->height, 1);

        if (data_size > 0) {
          buffer->EnsureCapacity(data_size, false);
          av_image_copy_to_buffer(buffer->data(), data_size,
                                  const_cast<const uint8_t**>(frame->data),
                                  frame->linesize,
                                  static_cast<AVPixelFormat>(frame->format),
                                  frame->width, frame->height, 1);
          buffer->SetRange(0, data_size);
        }
      }

      av_frame_free(&frame);
      pushed_index = PushOutputBuffer(index);
    }
  }

  if (pushed_index != kInvalidIndex) {
    NotifyOutputBufferAvailable(pushed_index);
  }
}

}  // namespace media
}  // namespace ave
