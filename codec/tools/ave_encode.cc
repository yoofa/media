/*
 * ave_encode.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "base/errors.h"
#include "base/logging.h"
#include "base/units/time_delta.h"
#include "media/audio/channel_layout.h"
#include "media/codec/codec.h"
#include "media/codec/codec_factory.h"
#include "media/codec/codec_id.h"
#include "media/codec/default_codec_factory.h"
#include "media/codec/simple_passthrough_codec.h"
#include "media/foundation/media_meta.h"

using namespace ave;
using namespace ave::media;

class EncoderCallback : public CodecCallback {
 public:
  EncoderCallback() = default;

  void OnInputBufferAvailable(size_t index) override {
    AVE_LOG(LS_VERBOSE) << "Input buffer available: " << index;
    input_available_indices_.push_back(index);
  }

  void OnOutputBufferAvailable(size_t index) override {
    AVE_LOG(LS_VERBOSE) << "Output buffer available: " << index;
    output_available_indices_.push_back(index);
  }

  void OnOutputFormatChanged(
      const std::shared_ptr<MediaMeta>& format) override {
    AVE_LOG(LS_INFO) << "Output format changed";
  }

  void OnError(status_t error) override {
    AVE_LOG(LS_ERROR) << "Encoder error: " << error;
    has_error_ = true;
  }

  void OnFrameRendered(std::shared_ptr<Message> notify) override {
    AVE_LOG(LS_VERBOSE) << "Frame rendered";
  }

  std::vector<size_t> input_available_indices_;
  std::vector<size_t> output_available_indices_;
  bool has_error_ = false;
};

void PrintUsage(const char* program_name) {
  std::cout
      << "Usage: " << program_name
      << " --type <codec_type> [--passthrough] <input_file> <output_file> "
         "[options]\n";
  std::cout << "\nCodec types:\n";
  std::cout << "  Audio: aac, opus, mp3\n";
  std::cout << "  Video: h264, h265, vp8, vp9\n";
  std::cout << "\nOptions:\n";
  std::cout << "  --passthrough       Use SimplePassthroughCodec (no actual "
               "encoding)\n";
  std::cout << "  --width <w>         Video width (default: 1920)\n";
  std::cout << "  --height <h>        Video height (default: 1080)\n";
  std::cout << "  --fps <fps>         Video frame rate (default: 30)\n";
  std::cout << "  --bitrate <bps>     Bitrate in bps (default: 2M video, 128k "
               "audio)\n";
  std::cout << "  --sample_rate <sr>  Audio sample rate (default: 48000)\n";
  std::cout << "  --channels <ch>     Audio channel count (default: 2)\n";
  std::cout << "\nExamples:\n";
  std::cout << "  # Normal encoding (FFmpeg)\n";
  std::cout << "  " << program_name << " --type aac input.pcm output.aac\n";
  std::cout << "  " << program_name
            << " --type h264 --width 1280 --height 720 input.yuv output.h264\n";
  std::cout << "\n  # Passthrough mode (PCM frames passthrough, no encoding)\n";
  std::cout << "  " << program_name
            << " --type aac --passthrough input.pcm output.pcm\n";
  std::cout << "\n  # Passthrough mode (YUV frames passthrough, no encoding)\n";
  std::cout << "  " << program_name
            << " --type h264 --passthrough input.yuv output.yuv\n";
  std::cout << "\nNote: In passthrough mode, data is copied frame by frame "
               "without encoding.\n";
}

CodecId GetCodecIdFromType(const std::string& type) {
  if (type == "aac") {
    return CodecId::AVE_CODEC_ID_AAC;
  } else if (type == "opus") {
    return CodecId::AVE_CODEC_ID_OPUS;
  } else if (type == "mp3") {
    return CodecId::AVE_CODEC_ID_MP3;
  } else if (type == "h264" || type == "avc") {
    return CodecId::AVE_CODEC_ID_H264;
  } else if (type == "h265" || type == "hevc") {
    return CodecId::AVE_CODEC_ID_HEVC;
  } else if (type == "vp8") {
    return CodecId::AVE_CODEC_ID_VP8;
  } else if (type == "vp9") {
    return CodecId::AVE_CODEC_ID_VP9;
  }
  return CodecId::AVE_CODEC_ID_NONE;
}

bool IsAudioCodec(CodecId codec_id) {
  return static_cast<uint32_t>(codec_id) >= 0x10000 &&
         static_cast<uint32_t>(codec_id) < 0x17000;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    PrintUsage(argv[0]);
    return 1;
  }

  ave::base::LogMessage::LogToDebug(ave::base::LogSeverity::LS_INFO);

  std::string codec_type;
  std::string input_file;
  std::string output_file;
  bool use_passthrough = false;

  int width = 1920;
  int height = 1080;
  int fps = 30;
  int bitrate = 0;  // 0 means use default based on type
  int sample_rate = 48000;
  int channels = 2;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--type" && i + 1 < argc) {
      codec_type = argv[++i];
    } else if (arg == "--simple") {
      // Kept for backward compatibility, but ignored
      continue;
    } else if (arg == "--passthrough") {
      use_passthrough = true;
    } else if (arg == "--width" && i + 1 < argc) {
      width = std::stoi(argv[++i]);
    } else if (arg == "--height" && i + 1 < argc) {
      height = std::stoi(argv[++i]);
    } else if (arg == "--fps" && i + 1 < argc) {
      fps = std::stoi(argv[++i]);
    } else if (arg == "--bitrate" && i + 1 < argc) {
      bitrate = std::stoi(argv[++i]);
    } else if (arg == "--sample_rate" && i + 1 < argc) {
      sample_rate = std::stoi(argv[++i]);
    } else if (arg == "--channels" && i + 1 < argc) {
      channels = std::stoi(argv[++i]);
    } else if (input_file.empty()) {
      input_file = arg;
    } else if (output_file.empty()) {
      output_file = arg;
    }
  }

  if (codec_type.empty() || input_file.empty() || output_file.empty()) {
    PrintUsage(argv[0]);
    return 1;
  }

  CodecId codec_id = GetCodecIdFromType(codec_type);
  if (codec_id == CodecId::AVE_CODEC_ID_NONE) {
    AVE_LOG(LS_ERROR) << "Unknown codec type: " << codec_type;
    return 1;
  }

  // Create codec
  std::shared_ptr<Codec> codec;

  if (use_passthrough) {
    // Use passthrough codec (no actual encoding, but uses same data flow)
    AVE_LOG(LS_INFO) << "Using SimplePassthroughCodec with type " << codec_type
                     << ": " << input_file << " -> " << output_file;
    codec = std::make_shared<SimplePassthroughCodec>(true);
  } else {
    // Initialize codec factory
    // Initialize codec factory (Default codec factory)
    auto default_factory = std::make_shared<DefaultCodecFactory>();
    RegisterCodecFactory(default_factory);

    AVE_LOG(LS_INFO) << "Encoding " << input_file << " to " << output_file
                     << " using codec: " << codec_type;

    codec = CreateCodecByType(codec_id, true);
    if (!codec) {
      AVE_LOG(LS_ERROR) << "Failed to create encoder for codec: " << codec_type;
      return 1;
    }
  }

  // Open input and output files
  std::ifstream input(input_file, std::ios::binary);
  if (!input) {
    AVE_LOG(LS_ERROR) << "Failed to open input file: " << input_file;
    return 1;
  }

  std::ofstream output(output_file, std::ios::binary);
  if (!output) {
    AVE_LOG(LS_ERROR) << "Failed to open output file: " << output_file;
    return 1;
  }

  // Configure codec
  auto config = std::make_shared<CodecConfig>();
  auto format = std::make_shared<MediaMeta>();

  bool is_audio = IsAudioCodec(codec_id);
  if (is_audio) {
    format->SetSampleRate(sample_rate);
    if (channels == 1) {
      format->SetChannelLayout(CHANNEL_LAYOUT_MONO);
    } else {
      format->SetChannelLayout(CHANNEL_LAYOUT_STEREO);
    }
    format->SetBitrate(bitrate > 0 ? bitrate : 128000);
  } else {
    format->SetWidth(width);
    format->SetHeight(height);
    format->SetFrameRate(fps);
    format->SetBitrate(bitrate > 0 ? bitrate : 2000000);
  }

  config->format = format;
  status_t result = codec->Configure(config);
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to configure encoder: " << result;
    return 1;
  }

  // Set callback
  EncoderCallback callback;
  result = codec->SetCallback(&callback);
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to set callback: " << result;
    return 1;
  }

  // Start codec
  result = codec->Start();
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to start encoder: " << result;
    return 1;
  }

  AVE_LOG(LS_INFO) << "Encoder started successfully";

  // Read input and feed to encoder
  const size_t buffer_size = is_audio ? 4096 : (width * height * 3 / 2);
  std::vector<uint8_t> read_buffer(buffer_size);
  bool input_eos = false;
  int64_t frame_count = 0;

  while (!input_eos || !callback.output_available_indices_.empty()) {
    // Feed input
    if (!input_eos) {
      ssize_t input_index = codec->DequeueInputBuffer(-1);
      if (input_index >= 0) {
        std::shared_ptr<CodecBuffer> input_buffer;
        result = codec->GetInputBuffer(input_index, input_buffer);
        if (result == OK && input_buffer) {
          input.read(reinterpret_cast<char*>(read_buffer.data()), buffer_size);
          size_t bytes_read = input.gcount();

          if (bytes_read > 0) {
            input_buffer->EnsureCapacity(bytes_read, true);
            std::memcpy(input_buffer->data(), read_buffer.data(), bytes_read);
            input_buffer->SetRange(0, bytes_read);

            auto meta = std::make_shared<MediaMeta>();
            meta->SetDuration(
                base::TimeDelta::Micros(frame_count * 1000000 / 30));
            input_buffer->format() = meta;

            result = codec->QueueInputBuffer(input_index);
            if (result != OK) {
              AVE_LOG(LS_ERROR) << "Failed to queue input buffer: " << result;
              break;
            }
            frame_count++;
          }

          if (input.eof() || bytes_read == 0) {
            input_eos = true;
            auto eos_meta = std::make_shared<MediaMeta>();
            // Set EOS flag somehow - need to check the proper API
            input_buffer->format() = eos_meta;
            codec->QueueInputBuffer(input_index);
            AVE_LOG(LS_INFO) << "Input end of stream, processed " << frame_count
                             << " frames";
          }
        }
      }
    }

    // Get output
    ssize_t output_index = codec->DequeueOutputBuffer(-1);
    if (output_index >= 0) {
      std::shared_ptr<CodecBuffer> output_buffer;
      result = codec->GetOutputBuffer(output_index, output_buffer);
      if (result == OK && output_buffer) {
        size_t size = output_buffer->size();

        if (size > 0) {
          output.write(reinterpret_cast<const char*>(output_buffer->data()),
                       size);
          AVE_LOG(LS_VERBOSE) << "Wrote " << size << " bytes to output";
        }

        codec->ReleaseOutputBuffer(output_index, false);
      }
    }

    if (callback.has_error_) {
      AVE_LOG(LS_ERROR) << "Encoding error occurred";
      break;
    }
  }

  // Stop codec
  codec->Stop();
  codec->Release();

  input.close();
  output.close();

  AVE_LOG(LS_INFO) << "Encoding completed. Total frames: " << frame_count;
  return 0;
}
