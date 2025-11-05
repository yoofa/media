/*
 * example.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Example usage of MPEG2-TS parser
 */

#include "ts_parser.h"

#include <fstream>
#include <iostream>

#include "base/logging.h"
#include "foundation/media_frame.h"

using namespace ave::media::mpeg2ts;

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <ts_file>" << std::endl;
    return 1;
  }

  // Open TS file
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open file: " << argv[1] << std::endl;
    return 1;
  }

  // Create parser
  auto parser = std::make_shared<TSParser>();

  // Buffer for TS packets (188 bytes each)
  constexpr size_t kTSPacketSize = 188;
  uint8_t packet[kTSPacketSize];

  size_t packet_count = 0;
  size_t video_frames = 0;
  size_t audio_frames = 0;

  // Read and parse TS packets
  while (file.read(reinterpret_cast<char*>(packet), kTSPacketSize)) {
    TSParser::SyncEvent event(packet_count * kTSPacketSize);
    status_t err = parser->FeedTSPacket(packet, kTSPacketSize, &event);

    if (err != OK) {
      LOG(ERROR) << "Failed to parse TS packet " << packet_count
                 << ": error=" << err;
      continue;
    }

    if (event.HasReturnedData()) {
      LOG(INFO) << "Sync event at packet " << packet_count
                << ", type=" << static_cast<int>(event.GetType())
                << ", time=" << event.GetTimeUs() << " us";
    }

    ++packet_count;

    // Periodically check for available data
    if (packet_count % 100 == 0) {
          // Check for video
      auto video_source = parser->GetSource(TSParser::VIDEO);
      if (video_source) {
        std::shared_ptr<MediaFrame> frame;
        while (video_source->Read(frame, nullptr) == OK) {
          ++video_frames;
          LOG(VERBOSE) << "Got video frame: size=" << frame->size()
                       << ", pts=" << frame->pts().us();
        }
      }

      // Check for audio
      auto audio_source = parser->GetSource(TSParser::AUDIO);
      if (audio_source) {
        std::shared_ptr<MediaFrame> frame;
        while (audio_source->Read(frame, nullptr) == OK) {
          ++audio_frames;
          LOG(VERBOSE) << "Got audio frame: size=" << frame->size()
                       << ", pts=" << frame->pts().us();
        }
      }
    }
  }

  // Signal EOS
  parser->SignalEOS(ERROR_END_OF_STREAM);

  // Read remaining frames
  auto video_source = parser->GetSource(TSParser::VIDEO);
  if (video_source) {
    std::shared_ptr<MediaFrame> frame;
    while (video_source->Read(frame, nullptr) == OK) {
      ++video_frames;
    }
  }

  auto audio_source = parser->GetSource(TSParser::AUDIO);
  if (audio_source) {
    std::shared_ptr<MediaFrame> frame;
    while (audio_source->Read(frame, nullptr) == OK) {
      ++audio_frames;
    }
  }

  // Print statistics
  std::cout << "\n=== MPEG2-TS Parsing Statistics ===" << std::endl;
  std::cout << "Total TS packets: " << packet_count << std::endl;
  std::cout << "Video frames: " << video_frames << std::endl;
  std::cout << "Audio frames: " << audio_frames << std::endl;
  std::cout << "Has video source: "
            << (parser->HasSource(TSParser::VIDEO) ? "Yes" : "No")
            << std::endl;
  std::cout << "Has audio source: "
            << (parser->HasSource(TSParser::AUDIO) ? "Yes" : "No")
            << std::endl;

  // Get format information
  if (video_source) {
    auto format = video_source->GetFormat();
    if (format) {
      std::cout << "\nVideo Format:" << std::endl;
      std::cout << "  Codec: " << format->mime() << std::endl;
      std::cout << "  Width: " << format->width() << std::endl;
      std::cout << "  Height: " << format->height() << std::endl;
      std::cout << "  FPS: " << format->fps() << std::endl;
    }
  }

  if (audio_source) {
    auto format = audio_source->GetFormat();
    if (format) {
      std::cout << "\nAudio Format:" << std::endl;
      std::cout << "  Codec: " << format->mime() << std::endl;
      std::cout << "  Sample Rate: " << format->sample_rate() << " Hz"
                << std::endl;
      std::cout << "  Channels: "
                << static_cast<int>(format->channel_layout()) << std::endl;
    }
  }

  return 0;
}
