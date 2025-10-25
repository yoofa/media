/*
 * ave_decode.cc
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
#include "media/codec/ffmpeg/ffmpeg_codec_factory.h"
#include "media/foundation/aac_utils.h"
#include "media/foundation/framing_queue.h"
#include "media/foundation/media_meta.h"

using namespace ave;
using namespace ave::media;

class DecoderCallback : public CodecCallback {
 public:
  DecoderCallback() = default;

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
    AVE_LOG(LS_ERROR) << "Decoder error: " << error;
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
  std::cout << "Usage: " << program_name
            << " --type <codec_type> <input_file> <output_file>\n";
  std::cout << "Codec types:\n";
  std::cout << "  Audio: aac, opus, mp3\n";
  std::cout << "  Video: h264, h265, vp8, vp9\n";
  std::cout << "\nExample:\n";
  std::cout << "  " << program_name << " --type aac input.aac output.pcm\n";
  std::cout << "  " << program_name << " --type h264 input.h264 output.yuv\n";
}

CodecId GetCodecIdFromType(const std::string& type) {
  if (type == "aac") {
    return CodecId::AVE_CODEC_ID_AAC;
  }
  if (type == "opus") {
    return CodecId::AVE_CODEC_ID_OPUS;
  }
  if (type == "mp3") {
    return CodecId::AVE_CODEC_ID_MP3;
  }
  if (type == "h264" || type == "avc") {
    return CodecId::AVE_CODEC_ID_H264;
  }
  if (type == "h265" || type == "hevc") {
    return CodecId::AVE_CODEC_ID_HEVC;
  }
  if (type == "vp8") {
    return CodecId::AVE_CODEC_ID_VP8;
  }
  if (type == "vp9") {
    return CodecId::AVE_CODEC_ID_VP9;
  }
  return CodecId::AVE_CODEC_ID_NONE;
}

bool IsAudioCodec(CodecId codec_id) {
  return static_cast<uint32_t>(codec_id) >= 0x10000 &&
         static_cast<uint32_t>(codec_id) < 0x17000;
}

FramingQueue::CodecType GetFramingCodecType(const std::string& type) {
  if (type == "aac") {
    return FramingQueue::CodecType::kAAC;
  }

  if (type == "h264" || type == "avc") {
    return FramingQueue::CodecType::kH264;
  }
  // Default to H264 for other video codecs (might need adjustment)
  return FramingQueue::CodecType::kH264;
}

int main(int argc, char** argv) {
  // Initialize codec factory
  auto ffmpeg_factory = std::make_shared<FFmpegCodecFactory>();
  RegisterCodecFactory(ffmpeg_factory);

  if (argc < 4) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string codec_type;
  std::string input_file;
  std::string output_file;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--type" && i + 1 < argc) {
      codec_type = argv[++i];
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

  AVE_LOG(LS_INFO) << "Decoding " << input_file << " to " << output_file
                   << " using codec: " << codec_type;

  // Create codec
  auto codec = CreateCodecByType(codec_id, false);
  if (!codec) {
    AVE_LOG(LS_ERROR) << "Failed to create decoder for codec: " << codec_type;
    return 1;
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

  // Set codec ID - decoder will determine other parameters from the bitstream
  format->SetCodec(codec_id);

  if (is_audio) {
    format->SetStreamType(MediaType::AUDIO);
    // For AAC with ADTS, we need to parse the first frame to get audio info
    if (codec_type == "aac") {
      // Read a small amount of data to parse ADTS header
      const size_t header_buffer_size = 1024;
      std::vector<uint8_t> header_buffer(header_buffer_size);
      input.read(reinterpret_cast<char*>(header_buffer.data()),
                 header_buffer_size);
      size_t bytes_read = input.gcount();

      if (bytes_read >= 7) {
        const uint8_t* data = header_buffer.data();
        size_t size = bytes_read;
        const uint8_t* frameStart = nullptr;
        size_t frameSize = 0;

        if (GetNextAACFrame(&data, &size, &frameStart, &frameSize) == OK) {
          ADTSHeader adts_header{};
          if (ParseADTSHeader(frameStart, frameSize, &adts_header) == OK) {
            uint32_t sample_rate =
                GetSamplingRate(adts_header.sampling_freq_index);
            uint8_t channel_count = GetChannelCount(adts_header.channel_config);

            AVE_LOG(LS_INFO) << "Detected AAC format: " << sample_rate << "Hz, "
                             << channel_count << " channels";

            format->SetSampleRate(sample_rate);
            // Map channel count to channel layout
            if (channel_count == 1) {
              format->SetChannelLayout(CHANNEL_LAYOUT_MONO);
            } else if (channel_count == 2) {
              format->SetChannelLayout(CHANNEL_LAYOUT_STEREO);
            } else {
              AVE_LOG(LS_WARNING)
                  << "Unsupported channel count: " << channel_count;
              format->SetChannelLayout(CHANNEL_LAYOUT_STEREO);
            }
          }
        }
      }

      // Reset file position to beginning
      input.clear();
      input.seekg(0, std::ios::beg);
    }
    // For decoders, don't set other parameters
    // Let the decoder detect them from the bitstream
  } else {
    format->SetStreamType(MediaType::VIDEO);
    // For decoders, don't set width/height etc.
    // Let the decoder detect them from the bitstream
  }

  config->format = format;
  status_t result = codec->Configure(config);
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to configure decoder: " << result;
    return 1;
  }

  // Set callback
  DecoderCallback callback;
  result = codec->SetCallback(&callback);
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to set callback: " << result;
    return 1;
  }

  // Start codec
  result = codec->Start();
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to start decoder: " << result;
    return 1;
  }

  AVE_LOG(LS_INFO) << "Decoder started successfully";

  // Create framing queue for parsing frames from raw bitstream
  FramingQueue::CodecType framing_type = GetFramingCodecType(codec_type);
  FramingQueue framing_queue(framing_type);

  // Read input and feed to framing queue, then decode frames
  const size_t buffer_size = 65536;
  std::vector<uint8_t> read_buffer(buffer_size);
  bool input_eos = false;
  bool eos_sent = false;
  int64_t frame_count = 0;
  int64_t packet_count = 0;

  while (!eos_sent || framing_queue.HasFrame() ||
         !callback.output_available_indices_.empty()) {
    // Read more data from input file
    if (!input_eos && !input.eof()) {
      input.read(reinterpret_cast<char*>(read_buffer.data()), buffer_size);
      size_t bytes_read = input.gcount();

      if (bytes_read > 0) {
        // Push data to framing queue
        framing_queue.PushData(read_buffer.data(), bytes_read);
        AVE_LOG(LS_INFO) << "Pushed " << bytes_read
                         << " bytes to framing queue, frame count: "
                         << framing_queue.FrameCount();
        packet_count++;
      }

      if (input.eof()) {
        input_eos = true;
        AVE_LOG(LS_INFO) << "Input end of stream, read " << packet_count
                         << " packets";
      }
    }

    // Feed frames to decoder
    while (framing_queue.HasFrame()) {
      AVE_LOG(LS_INFO)
          << "Framing queue has frames, trying to dequeue input buffer";
      ssize_t input_index = codec->DequeueInputBuffer(-1);
      if (input_index < 0) {
        AVE_LOG(LS_INFO) << "No input buffer available";
        break;  // No input buffer available, will try again later
      }

      std::shared_ptr<CodecBuffer> input_buffer;
      result = codec->GetInputBuffer(input_index, input_buffer);
      if (result != OK || !input_buffer) {
        AVE_LOG(LS_ERROR) << "Failed to get input buffer";
        break;
      }

      auto frame = framing_queue.PopFrame();
      if (!frame) {
        AVE_LOG(LS_WARNING)
            << "PopFrame returned null even though HasFrame was true";
        break;
      }

      AVE_LOG(LS_INFO) << "Got frame from queue with size: " << frame->size();

      // Copy frame data to codec input buffer
      input_buffer->EnsureCapacity(frame->size(), true);
      std::memcpy(input_buffer->data(), frame->data(), frame->size());
      input_buffer->SetRange(0, frame->size());

      auto meta = std::make_shared<MediaMeta>();
      meta->SetDuration(base::TimeDelta::Micros(frame_count * 1000000 / 30));
      input_buffer->format() = meta;

      result = codec->QueueInputBuffer(input_index);
      if (result != OK) {
        AVE_LOG(LS_ERROR) << "Failed to queue input buffer: " << result;
        break;
      }
      AVE_LOG(LS_INFO) << "Successfully queued input buffer";
      frame_count++;
      AVE_LOG(LS_INFO) << "Queued frame " << frame_count << " ("
                       << frame->size() << " bytes)";
    }

    // Send EOS to decoder if input is done and no more frames
    if (input_eos && !framing_queue.HasFrame() && !eos_sent) {
      ssize_t input_index = codec->DequeueInputBuffer(-1);
      if (input_index >= 0) {
        std::shared_ptr<CodecBuffer> input_buffer;
        result = codec->GetInputBuffer(input_index, input_buffer);
        if (result == OK && input_buffer) {
          auto eos_meta = std::make_shared<MediaMeta>();
          input_buffer->SetRange(0, 0);
          input_buffer->format() = eos_meta;
          codec->QueueInputBuffer(input_index);
          AVE_LOG(LS_INFO) << "Sent EOS to decoder after " << frame_count
                           << " frames";
          eos_sent = true;
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
          AVE_LOG(LS_INFO) << "Wrote " << size << " bytes to output";
        } else {
          AVE_LOG(LS_WARNING) << "Got output buffer but size is 0";
        }

        codec->ReleaseOutputBuffer(output_index, false);
      }
    } else if (eos_sent && !framing_queue.HasFrame()) {
      // If EOS was sent and no output available, decoding is done
      AVE_LOG(LS_INFO) << "Decoding completed, no more output";
      break;
    }

    if (callback.has_error_) {
      AVE_LOG(LS_ERROR) << "Decoding error occurred";
      break;
    }
  }

  // Stop codec
  codec->Stop();
  codec->Release();

  input.close();
  output.close();

  AVE_LOG(LS_INFO) << "Decoding completed. Total packets: " << frame_count;
  return 0;
}
