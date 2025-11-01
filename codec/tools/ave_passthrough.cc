/*
 * ave_passthrough.cc
 * Simple tool to test SimplePassthroughCodec
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "base/errors.h"
#include "base/logging.h"
#include "media/codec/codec.h"
#include "media/codec/simple_passthrough_codec.h"
#include "media/foundation/media_meta.h"

using namespace ave;
using namespace ave::media;

class PassthroughCallback : public CodecCallback {
 public:
  void OnInputBufferAvailable(size_t index) override {
    AVE_LOG(LS_VERBOSE) << "Input buffer available: " << index;
  }

  void OnOutputBufferAvailable(size_t index) override {
    AVE_LOG(LS_VERBOSE) << "Output buffer available: " << index;
  }

  void OnOutputFormatChanged(
      const std::shared_ptr<MediaMeta>& format) override {
    AVE_LOG(LS_INFO) << "Output format changed";
  }

  void OnError(status_t error) override {
    AVE_LOG(LS_ERROR) << "Error: " << error;
    has_error_ = true;
  }

  void OnFrameRendered(std::shared_ptr<Message> notify) override {}

  bool has_error_ = false;
};

void PrintUsage(const char* program_name) {
  std::cout << "Usage: " << program_name << " <input_file> <output_file>\n";
  std::cout << "\nDescription:\n";
  std::cout << "  This tool uses SimplePassthroughCodec to copy data from\n";
  std::cout << "  input file to output file without any encoding/decoding.\n";
  std::cout << "  The output will be identical to the input.\n";
  std::cout << "\nExample:\n";
  std::cout << "  " << program_name << " input.dat output.dat\n";
  std::cout << "  " << program_name << " test.bin test_copy.bin\n";
}

int main(int argc, char** argv) {
  if (argc != 3) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string input_file = argv[1];
  std::string output_file = argv[2];

  ave::base::LogMessage::LogToDebug(ave::base::LogSeverity::LS_INFO);

  AVE_LOG(LS_INFO) << "SimplePassthroughCodec: " << input_file << " -> "
                   << output_file;

  // Open files
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

  // Create passthrough codec
  auto codec = std::make_shared<SimplePassthroughCodec>(false);

  // Configure
  auto config = std::make_shared<CodecConfig>();
  auto format = MediaMeta::CreatePtr();
  format->SetStreamType(MediaType::AUDIO);
  config->format = format;

  status_t result = codec->Configure(config);
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to configure codec: " << result;
    return 1;
  }

  // Set callback
  PassthroughCallback callback;
  codec->SetCallback(&callback);

  // Start
  result = codec->Start();
  if (result != OK) {
    AVE_LOG(LS_ERROR) << "Failed to start codec: " << result;
    return 1;
  }

  AVE_LOG(LS_INFO) << "Codec started, processing data...";

  // Process data
  const size_t chunk_size = 4096;
  std::vector<uint8_t> read_buffer(chunk_size);
  bool input_eos = false;
  bool output_eos = false;
  int64_t frames_processed = 0;
  int64_t frames_output = 0;
  int64_t total_bytes_input = 0;
  int64_t total_bytes_output = 0;

  while (!output_eos) {
    // Read and queue input
    if (!input_eos) {
      input.read(reinterpret_cast<char*>(read_buffer.data()), chunk_size);
      size_t bytes_read = input.gcount();

      if (bytes_read > 0) {
        // Get input buffer
        ssize_t input_index = codec->DequeueInputBuffer(1000);
        if (input_index < 0) {
          AVE_LOG(LS_ERROR) << "Failed to dequeue input buffer";
          break;
        }

        std::shared_ptr<CodecBuffer> input_buffer;
        result = codec->GetInputBuffer(input_index, input_buffer);
        if (result != OK) {
          AVE_LOG(LS_ERROR) << "Failed to get input buffer";
          break;
        }

        // Copy data
        input_buffer->EnsureCapacity(bytes_read, true);
        std::memcpy(input_buffer->data(), read_buffer.data(), bytes_read);
        input_buffer->SetRange(0, bytes_read);

        // Queue
        result = codec->QueueInputBuffer(input_index);
        if (result != OK) {
          AVE_LOG(LS_ERROR) << "Failed to queue input buffer";
          break;
        }

        frames_processed++;
        total_bytes_input += bytes_read;
        AVE_LOG(LS_VERBOSE) << "Queued frame " << frames_processed << " ("
                            << bytes_read << " bytes)";
      }

      if (input.eof() || bytes_read == 0) {
        input_eos = true;
        AVE_LOG(LS_INFO) << "Input complete, " << total_bytes_input
                         << " bytes processed";

        // Send EOS
        ssize_t input_index = codec->DequeueInputBuffer(100);
        if (input_index >= 0) {
          std::shared_ptr<CodecBuffer> input_buffer;
          result = codec->GetInputBuffer(input_index, input_buffer);
          if (result == OK) {
            input_buffer->SetRange(0, 0);
            input_buffer->format()->SetEos(true);
            codec->QueueInputBuffer(input_index);
            AVE_LOG(LS_INFO) << "Sent EOS to codec";
          }
        }
      }
    }

    // Get output
    int timeout = input_eos ? 1000 : 10;
    ssize_t output_index = codec->DequeueOutputBuffer(timeout);

    if (output_index >= 0) {
      std::shared_ptr<CodecBuffer> output_buffer;
      result = codec->GetOutputBuffer(output_index, output_buffer);
      if (result == OK && output_buffer) {
        size_t size = output_buffer->size();

        if (size > 0) {
          output.write(reinterpret_cast<const char*>(output_buffer->data()),
                       size);
          frames_output++;
          total_bytes_output += size;
          AVE_LOG(LS_VERBOSE)
              << "Output frame " << frames_output << " (" << size << " bytes)";
        } else if (input_eos) {
          AVE_LOG(LS_INFO) << "Received EOS from codec";
          output_eos = true;
        }

        codec->ReleaseOutputBuffer(output_index, false);
      }
    } else if (input_eos) {
      AVE_LOG(LS_INFO) << "No more output after EOS";
      output_eos = true;
    }

    if (callback.has_error_) {
      AVE_LOG(LS_ERROR) << "Error occurred during processing";
      break;
    }
  }

  // Stop codec
  codec->Stop();
  codec->Release();

  // Close files
  input.close();
  output.close();

  // Print statistics
  AVE_LOG(LS_INFO) << "Processing complete!";
  AVE_LOG(LS_INFO) << "  Input frames:  " << frames_processed;
  AVE_LOG(LS_INFO) << "  Output frames: " << frames_output;
  AVE_LOG(LS_INFO) << "  Input bytes:   " << total_bytes_input;
  AVE_LOG(LS_INFO) << "  Output bytes:  " << total_bytes_output;

  // Verify
  if (total_bytes_input == total_bytes_output) {
    std::cout << "\n✓ SUCCESS: Input and output sizes match!" << std::endl;
    std::cout << "  Processed " << total_bytes_input << " bytes" << std::endl;
    return 0;
  }

  std::cout << "\n✗ ERROR: Size mismatch!" << std::endl;
  std::cout << "  Input:  " << total_bytes_input << " bytes" << std::endl;
  std::cout << "  Output: " << total_bytes_output << " bytes" << std::endl;
  return 1;
}
