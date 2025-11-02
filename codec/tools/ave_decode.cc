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
#include "media/codec/default_codec_factory.h"
#include "media/codec/simple_passthrough_codec.h"
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
  std::cout
      << "Usage: " << program_name
      << " --type <codec_type> [--passthrough] <input_file> <output_file> "
         "[options]\n";
  std::cout << "\nCodec types:\n";
  std::cout << "  Audio: aac, opus, mp3\n";
  std::cout << "  Video: h264, h265, vp8, vp9\n";
  std::cout << "\nOptions:\n";
  std::cout << "  --passthrough       Use SimplePassthroughCodec (no actual "
               "decoding)\n";
  std::cout << "  --width <w>         Video width (optional hint)\n";
  std::cout << "  --height <h>        Video height (optional hint)\n";
  std::cout << "  --sample_rate <sr>  Audio sample rate (optional hint)\n";
  std::cout << "  --channels <ch>     Audio channel count (optional hint)\n";
  std::cout << "  --force_s16         Force output to 16-bit PCM (convert if "
               "needed)\n";
  std::cout << "\nExamples:\n";
  std::cout << "  # Normal decoding (FFmpeg)\n";
  std::cout << "  " << program_name << " --type aac input.aac output.pcm\n";
  std::cout << "\n  # Passthrough mode (AAC frames passthrough, no decoding)\n";
  std::cout << "  " << program_name
            << " --type aac --passthrough input.aac output.aac\n";
  std::cout
      << "\n  # Passthrough mode (H264 frames passthrough, no decoding)\n";
  std::cout << "  " << program_name
            << " --type h264 --passthrough input.h264 output.h264\n";
  std::cout
      << "\nNote: In passthrough mode, frames are extracted but not decoded.\n";
  std::cout << "      The output will be the same format as input (frame by "
               "frame).\n";
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
  if (argc < 3) {
    PrintUsage(argv[0]);
    return 1;
  }
  ave::base::LogMessage::LogToDebug(ave::base::LogSeverity::LS_VERBOSE);

  std::string codec_type;
  std::string input_file;
  std::string output_file;
  bool use_passthrough = false;
  bool force_s16 = false;

  int width = 0;
  int height = 0;
  int sample_rate = 0;
  int channels = 0;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--type" && i + 1 < argc) {
      codec_type = argv[++i];
    } else if (arg == "--simple") {
      // Kept for backward compatibility, but ignored
      continue;
    } else if (arg == "--passthrough") {
      use_passthrough = true;
    } else if (arg == "--force_s16") {
      force_s16 = true;
    } else if (arg == "--width" && i + 1 < argc) {
      width = std::stoi(argv[++i]);
    } else if (arg == "--height" && i + 1 < argc) {
      height = std::stoi(argv[++i]);
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

  if (input_file.empty() || output_file.empty()) {
    PrintUsage(argv[0]);
    return 1;
  }

  // For passthrough mode, codec type is still needed for framing
  if (use_passthrough && codec_type.empty()) {
    AVE_LOG(LS_ERROR)
        << "Codec type is required for --passthrough mode (for frame parsing)";
    PrintUsage(argv[0]);
    return 1;
  }

  // Create codec
  std::shared_ptr<Codec> codec;

  if (use_passthrough) {
    // Use passthrough codec (no actual decoding, but uses same data flow)
    AVE_LOG(LS_INFO) << "Using SimplePassthroughCodec with type " << codec_type
                     << ": " << input_file << " -> " << output_file;
    codec = std::make_shared<SimplePassthroughCodec>(false);
  } else {
    // Initialize codec factory (Default codec factory)
    auto default_factory = std::make_shared<DefaultCodecFactory>();
    RegisterCodecFactory(default_factory);

    if (codec_type.empty()) {
      AVE_LOG(LS_ERROR) << "Codec type is required";
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

    codec = CreateCodecByType(codec_id, false);
    if (!codec) {
      AVE_LOG(LS_ERROR) << "Failed to create decoder for codec: " << codec_type;
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

  // Get codec ID for both passthrough and normal modes
  CodecId codec_id = GetCodecIdFromType(codec_type);
  bool is_audio = IsAudioCodec(codec_id);

  // Set codec ID (needed for framing queue type detection)
  format->SetCodec(codec_id);
  format->SetStreamType(is_audio ? MediaType::AUDIO : MediaType::VIDEO);

  if (!use_passthrough) {
    // For real codec, parse format parameters
    if (is_audio) {
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
              uint32_t detected_sample_rate =
                  GetSamplingRate(adts_header.sampling_freq_index);
              uint8_t channel_count =
                  GetChannelCount(adts_header.channel_config);

              AVE_LOG(LS_INFO)
                  << "Detected AAC format: " << detected_sample_rate << "Hz, "
                  << channel_count << " channels";

              format->SetSampleRate(detected_sample_rate);
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
  }

  // Apply command line overrides/hints
  if (sample_rate > 0)
    format->SetSampleRate(sample_rate);
  if (channels > 0) {
    if (channels == 1) {
      format->SetChannelLayout(CHANNEL_LAYOUT_MONO);
    } else {
      format->SetChannelLayout(CHANNEL_LAYOUT_STEREO);
    }
  }
  if (width > 0)
    format->SetWidth(width);
  if (height > 0)
    format->SetHeight(height);

  // Default to 16-bit PCM output
  // format->SetBitsPerSample(16);

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

  // Main decode loop
  const size_t buffer_size = 65536;
  std::vector<uint8_t> read_buffer(buffer_size);
  bool input_eos = false;
  bool eos_sent = false;
  bool output_eos = false;
  int64_t frame_count = 0;
  int64_t output_frame_count = 0;

  AVE_LOG(LS_INFO) << "Starting decode loop";

  while (!output_eos) {
    // Step 1: Try to queue input buffer if we have data
    bool input_queued = false;

    // Read more data from file if framing queue is low
    if (!input_eos && framing_queue.FrameCount() < 5) {
      input.read(reinterpret_cast<char*>(read_buffer.data()), buffer_size);
      size_t bytes_read = input.gcount();

      if (bytes_read > 0) {
        framing_queue.PushData(read_buffer.data(), bytes_read);
        AVE_LOG(LS_VERBOSE)
            << "Pushed " << bytes_read << " bytes to framing queue";
      }

      if (input.eof()) {
        input_eos = true;
        AVE_LOG(LS_INFO) << "Input end of stream reached";
      }
    }

    // Try to queue one input frame
    if (framing_queue.HasFrame() && !eos_sent) {
      AVE_LOG(LS_INFO) << "Trying to queue input, frames available: "
                       << framing_queue.FrameCount();
      ssize_t input_index = codec->DequeueInputBuffer(10);
      AVE_LOG(LS_INFO) << "DequeueInputBuffer returned: " << input_index;
      if (input_index >= 0) {
        std::shared_ptr<CodecBuffer> input_buffer;
        result = codec->GetInputBuffer(input_index, input_buffer);
        if (result == OK && input_buffer) {
          auto frame = framing_queue.PopFrame();
          if (frame) {
            // Copy frame data to codec input buffer
            input_buffer->EnsureCapacity(frame->size(), true);
            std::memcpy(input_buffer->data(), frame->data(), frame->size());
            input_buffer->SetRange(0, frame->size());

            auto meta = MediaMeta::CreatePtr();
            meta->SetDuration(
                base::TimeDelta::Micros(frame_count * 1000000 / 30));
            input_buffer->format() = meta;

            result = codec->QueueInputBuffer(input_index);
            if (result == OK) {
              frame_count++;
              input_queued = true;
              AVE_LOG(LS_VERBOSE) << "Queued input frame " << frame_count
                                  << " (" << frame->size() << " bytes)";
            } else {
              AVE_LOG(LS_ERROR) << "Failed to queue input buffer: " << result;
            }
          }
        } else {
          AVE_LOG(LS_ERROR) << "Failed to get input buffer: " << result;
        }
      }
    } else if (input_eos && !framing_queue.HasFrame() && !eos_sent) {
      // Send EOS frame
      ssize_t input_index = codec->DequeueInputBuffer(10);
      if (input_index >= 0) {
        std::shared_ptr<CodecBuffer> input_buffer;
        result = codec->GetInputBuffer(input_index, input_buffer);
        if (result == OK && input_buffer) {
          // Send empty buffer to indicate EOS
          input_buffer->SetRange(0, 0);
          input_buffer->format() = MediaMeta::CreatePtr();
          codec->QueueInputBuffer(input_index);
          AVE_LOG(LS_INFO) << "Sent EOS to decoder after " << frame_count
                           << " frames";
          eos_sent = true;
        }
      }
    }

    // Step 2: Try to dequeue output buffer
    // Use longer timeout if we just queued input, shorter if waiting for more
    // output
    int output_timeout = (input_queued || !eos_sent) ? 10 : 100;
    AVE_LOG(LS_INFO) << "Trying to dequeue output, timeout: " << output_timeout;
    ssize_t output_index = codec->DequeueOutputBuffer(output_timeout);
    AVE_LOG(LS_INFO) << "DequeueOutputBuffer returned: " << output_index;

    if (output_index >= 0) {
      std::shared_ptr<CodecBuffer> output_buffer;
      result = codec->GetOutputBuffer(output_index, output_buffer);
      if (result == OK && output_buffer) {
        size_t size = output_buffer->size();

        if (size > 0) {
          // Check if conversion is needed
          if (force_s16 && output_buffer->format() &&
              output_buffer->format()->stream_type() == MediaType::AUDIO) {
            CodecId output_codec_id = output_buffer->format()->codec();

            if (output_codec_id == CodecId::AVE_CODEC_ID_PCM_F32LE) {
              // Convert Packed Float to Packed S16 in-place
              size_t sample_count = size / 4;
              float* src = reinterpret_cast<float*>(output_buffer->data());
              int16_t* dst = reinterpret_cast<int16_t*>(output_buffer->data());

              for (size_t i = 0; i < sample_count; ++i) {
                float val = src[i];
                if (val > 1.0f)
                  val = 1.0f;
                if (val < -1.0f)
                  val = -1.0f;
                dst[i] = static_cast<int16_t>(val * 32767.0f);
              }
              size = sample_count * 2;
              output_buffer->SetRange(0, size);
            }
          }

          output.write(reinterpret_cast<const char*>(output_buffer->data()),
                       size);
          output_frame_count++;
          AVE_LOG(LS_VERBOSE) << "Output frame " << output_frame_count << ": "
                              << size << " bytes";
        } else if (eos_sent) {
          // If we sent EOS and got empty buffer, consider it as EOS
          AVE_LOG(LS_INFO) << "Received empty buffer after EOS, decoding done";
          output_eos = true;
        }

        AVE_LOG(LS_VERBOSE) << "Released output buffer before " << output_index;
        codec->ReleaseOutputBuffer(output_index, false);
        AVE_LOG(LS_VERBOSE) << "Released output buffer " << output_index;
      }
    } else if (eos_sent) {
      // If we sent EOS and can't get more output, we're done
      AVE_LOG(LS_INFO) << "No more output after EOS";
      output_eos = true;
    }

    if (callback.has_error_) {
      AVE_LOG(LS_ERROR) << "Decoding error occurred";
      break;
    }
  }

  AVE_LOG(LS_INFO) << "Decoding completed. Input frames: " << frame_count
                   << ", Output frames: " << output_frame_count;

  // Stop codec
  codec->Stop();
  codec->Release();

  input.close();
  output.close();

  AVE_LOG(LS_INFO) << "Decoding completed. Total packets: " << frame_count;
  return 0;
}
