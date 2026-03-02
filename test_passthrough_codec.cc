/*
 * test_passthrough_codec.cc
 * Test program for SimplePassthroughCodec
 */

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "base/errors.h"
#include "base/logging.h"
#include "codec/codec.h"
#include "codec/simple_passthrough_codec.h"
#include "foundation/media_meta.h"

using namespace ave;
using namespace ave::media;

class TestCallback : public CodecCallback {
 public:
  void OnInputBufferAvailable(size_t index) override {
    std::cout << "✓ Input buffer " << index << " available" << std::endl;
    input_count++;
  }

  void OnOutputBufferAvailable(size_t index) override {
    std::cout << "✓ Output buffer " << index << " ready" << std::endl;
    output_count++;
  }

  void OnOutputFormatChanged(const std::shared_ptr<MediaMeta>& format) override {
    std::cout << "✓ Output format changed" << std::endl;
  }

  void OnError(status_t error) override {
    std::cout << "✗ Error: " << error << std::endl;
    has_error = true;
  }

  void OnFrameRendered(std::shared_ptr<Message> notify) override {}

  int input_count = 0;
  int output_count = 0;
  bool has_error = false;
};

int main(int argc, char** argv) {
  std::cout << "=== SimplePassthroughCodec Test ===" << std::endl;

  // Create passthrough codec (decoder mode, but doesn't matter for passthrough)
  auto codec = std::make_shared<SimplePassthroughCodec>(false);
  
  // Configure codec
  auto config = std::make_shared<CodecConfig>();
  auto format = MediaMeta::CreatePtr();
  format->SetStreamType(MediaType::AUDIO);
  config->format = format;

  std::cout << "\n1. Configuring codec..." << std::endl;
  if (codec->Configure(config) != OK) {
    std::cout << "✗ Failed to configure codec" << std::endl;
    return 1;
  }
  std::cout << "✓ Codec configured" << std::endl;

  // Set callback
  TestCallback callback;
  codec->SetCallback(&callback);
  std::cout << "✓ Callback set" << std::endl;

  // Start codec
  std::cout << "\n2. Starting codec..." << std::endl;
  if (codec->Start() != OK) {
    std::cout << "✗ Failed to start codec" << std::endl;
    return 1;
  }
  std::cout << "✓ Codec started" << std::endl;

  // Test data
  const int TEST_FRAMES = 5;
  std::vector<std::vector<uint8_t>> test_data;
  
  // Generate test data
  for (int i = 0; i < TEST_FRAMES; i++) {
    std::vector<uint8_t> data(100 + i * 10);  // Variable size
    for (size_t j = 0; j < data.size(); j++) {
      data[j] = static_cast<uint8_t>((i * 100 + j) % 256);
    }
    test_data.push_back(data);
  }

  std::cout << "\n3. Processing " << TEST_FRAMES << " frames..." << std::endl;
  
  int frames_processed = 0;
  int frames_output = 0;
  
  for (int i = 0; i < TEST_FRAMES; i++) {
    std::cout << "\n--- Frame " << (i + 1) << " ---" << std::endl;
    
    // Dequeue input buffer
    ssize_t input_index = codec->DequeueInputBuffer(1000);
    if (input_index < 0) {
      std::cout << "✗ Failed to dequeue input buffer" << std::endl;
      break;
    }
    std::cout << "✓ Got input buffer: " << input_index << std::endl;

    // Get input buffer
    std::shared_ptr<CodecBuffer> input_buffer;
    if (codec->GetInputBuffer(input_index, input_buffer) != OK) {
      std::cout << "✗ Failed to get input buffer" << std::endl;
      break;
    }

    // Write test data
    auto& data = test_data[i];
    input_buffer->EnsureCapacity(data.size(), true);
    std::memcpy(input_buffer->data(), data.data(), data.size());
    input_buffer->SetRange(0, data.size());
    std::cout << "✓ Wrote " << data.size() << " bytes to input buffer" << std::endl;

    // Queue input buffer
    if (codec->QueueInputBuffer(input_index) != OK) {
      std::cout << "✗ Failed to queue input buffer" << std::endl;
      break;
    }
    std::cout << "✓ Queued input buffer" << std::endl;
    frames_processed++;

    // Dequeue output buffer (with retry)
    ssize_t output_index = -1;
    for (int retry = 0; retry < 10; retry++) {
      output_index = codec->DequeueOutputBuffer(100);
      if (output_index >= 0) {
        break;
      }
    }

    if (output_index < 0) {
      std::cout << "⚠ No output buffer yet (may come later)" << std::endl;
      continue;
    }

    // Get output buffer
    std::shared_ptr<CodecBuffer> output_buffer;
    if (codec->GetOutputBuffer(output_index, output_buffer) != OK) {
      std::cout << "✗ Failed to get output buffer" << std::endl;
      break;
    }

    // Verify output data matches input data
    size_t output_size = output_buffer->size();
    std::cout << "✓ Got output buffer: " << output_index 
              << ", size: " << output_size << " bytes" << std::endl;

    bool data_matches = (output_size == data.size());
    if (data_matches) {
      data_matches = (std::memcmp(output_buffer->data(), data.data(), 
                                   data.size()) == 0);
    }

    if (data_matches) {
      std::cout << "✓ Output data matches input data!" << std::endl;
    } else {
      std::cout << "✗ Output data does NOT match input data!" << std::endl;
    }

    // Release output buffer
    codec->ReleaseOutputBuffer(output_index, false);
    std::cout << "✓ Released output buffer" << std::endl;
    frames_output++;
  }

  // Try to get any remaining output buffers
  std::cout << "\n4. Checking for remaining output..." << std::endl;
  while (frames_output < frames_processed) {
    ssize_t output_index = codec->DequeueOutputBuffer(100);
    if (output_index < 0) {
      break;
    }

    std::shared_ptr<CodecBuffer> output_buffer;
    if (codec->GetOutputBuffer(output_index, output_buffer) == OK) {
      std::cout << "✓ Got delayed output buffer: " << output_index 
                << ", size: " << output_buffer->size() << std::endl;
      codec->ReleaseOutputBuffer(output_index, false);
      frames_output++;
    }
  }

  // Stop codec
  std::cout << "\n5. Stopping codec..." << std::endl;
  codec->Stop();
  codec->Release();
  std::cout << "✓ Codec stopped and released" << std::endl;

  // Print statistics
  std::cout << "\n=== Test Results ===" << std::endl;
  std::cout << "Frames queued:     " << frames_processed << std::endl;
  std::cout << "Frames output:     " << frames_output << std::endl;
  std::cout << "Input callbacks:   " << callback.input_count << std::endl;
  std::cout << "Output callbacks:  " << callback.output_count << std::endl;
  std::cout << "Errors:            " << (callback.has_error ? "YES" : "NO") << std::endl;

  bool success = (frames_processed == TEST_FRAMES) && 
                 (frames_output == TEST_FRAMES) &&
                 !callback.has_error;

  if (success) {
    std::cout << "\n✓✓✓ ALL TESTS PASSED! ✓✓✓" << std::endl;
    return 0;
  } else {
    std::cout << "\n✗✗✗ SOME TESTS FAILED! ✗✗✗" << std::endl;
    return 1;
  }
}
