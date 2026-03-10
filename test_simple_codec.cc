/*
 * test_simple_codec.cc
 * Simple test to verify SimpleCodec decode workflow
 */

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "base/errors.h"
#include "base/logging.h"
#include "media/codec/codec.h"
#include "media/codec/codec_factory.h"
#include "media/codec/codec_id.h"
#include "media/codec/ffmpeg/ffmpeg_codec_factory.h"
#include "media/foundation/framing_queue.h"
#include "media/foundation/media_meta.h"

using namespace ave;
using namespace ave::media;

class TestCallback : public CodecCallback {
 public:
  void OnInputBufferAvailable(size_t index) override {
    std::cout << "Input buffer available: " << index << std::endl;
    input_count++;
  }

  void OnOutputBufferAvailable(size_t index) override {
    std::cout << "Output buffer available: " << index << std::endl;
    output_count++;
  }

  void OnOutputFormatChanged(
      const std::shared_ptr<MediaMeta>& format) override {
    std::cout << "Output format changed" << std::endl;
  }

  void OnError(status_t error) override {
    std::cout << "Error: " << error << std::endl;
  }

  void OnFrameRendered(std::shared_ptr<Message> notify) override {}

  int input_count = 0;
  int output_count = 0;
};

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <input.aac>" << std::endl;
    return 1;
  }

  // Register simple codec factory
  auto factory = std::make_shared<FFmpegCodecFactory>(true);
  RegisterCodecFactory(factory);

  // Create decoder
  auto codec = CreateCodecByType(CodecId::AVE_CODEC_ID_AAC, false);
  if (!codec) {
    std::cout << "Failed to create decoder" << std::endl;
    return 1;
  }

  // Configure
  auto config = std::make_shared<CodecConfig>();
  auto format = std::make_shared<MediaMeta>();
  format->SetCodec(CodecId::AVE_CODEC_ID_AAC);
  format->SetStreamType(MediaType::AUDIO);
  config->format = format;

  if (codec->Configure(config) != OK) {
    std::cout << "Failed to configure" << std::endl;
    return 1;
  }

  // Set callback
  TestCallback callback;
  codec->SetCallback(&callback);

  // Start
  if (codec->Start() != OK) {
    std::cout << "Failed to start" << std::endl;
    return 1;
  }

  std::cout << "Codec started successfully" << std::endl;

  // Read input file
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    std::cout << "Failed to open input file" << std::endl;
    return 1;
  }

  // Create framing queue
  FramingQueue framing_queue(FramingQueue::CodecType::kAAC);

  // Read all data
  std::vector<uint8_t> buffer(65536);
  input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
  size_t bytes_read = input.gcount();

  std::cout << "Read " << bytes_read << " bytes" << std::endl;

  // Push to framing queue
  framing_queue.PushData(buffer.data(), bytes_read);
  std::cout << "Framing queue has " << framing_queue.FrameCount() << " frames"
            << std::endl;

  // Process a few frames
  int frames_processed = 0;
  while (framing_queue.HasFrame() && frames_processed < 5) {
    auto frame = framing_queue.PopFrame();
    std::cout << "Frame " << frames_processed << " size: " << frame->size()
              << std::endl;

    // Dequeue input buffer
    ssize_t input_index = codec->DequeueInputBuffer(1000);
    if (input_index < 0) {
      std::cout << "Failed to dequeue input buffer after frame "
                << frames_processed << std::endl;
      break;
    }

    std::cout << "Got input buffer index: " << input_index << std::endl;

    // Get buffer
    std::shared_ptr<CodecBuffer> input_buffer;
    if (codec->GetInputBuffer(input_index, input_buffer) != OK) {
      std::cout << "Failed to get input buffer" << std::endl;
      break;
    }

    // Copy data
    input_buffer->EnsureCapacity(frame->size(), true);
    std::memcpy(input_buffer->data(), frame->data(), frame->size());
    input_buffer->SetRange(0, frame->size());

    // Queue
    if (codec->QueueInputBuffer(input_index) != OK) {
      std::cout << "Failed to queue input buffer" << std::endl;
      break;
    }

    std::cout << "Queued input buffer" << std::endl;
    frames_processed++;

    // Try to get output
    ssize_t output_index = codec->DequeueOutputBuffer(100);
    if (output_index >= 0) {
      std::shared_ptr<CodecBuffer> output_buffer;
      if (codec->GetOutputBuffer(output_index, output_buffer) == OK) {
        std::cout << "Got output buffer with size: " << output_buffer->size()
                  << std::endl;
        codec->ReleaseOutputBuffer(output_index, false);
      }
    }
  }

  std::cout << "Processed " << frames_processed << " frames" << std::endl;
  std::cout << "Input callbacks: " << callback.input_count << std::endl;
  std::cout << "Output callbacks: " << callback.output_count << std::endl;

  codec->Stop();
  codec->Release();

  return 0;
}
