/*
 * play_audio.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <memory>

#include "media/audio/audio_device.h"
#include "media/audio/audio_format.h"
#include "media/audio/audio_track.h"
#include "media/audio/audio.h"
#include "media/audio/channel_layout.h"

using ave::media::AudioDevice;
using ave::media::AudioConfig;
using ave::media::AudioFormat;
using ave::media::GuessChannelLayout;
using ave::media::AudioTrack;


namespace {
volatile bool g_running = true;

void SignalHandler(int signal) {
  if (signal == SIGINT) {
    g_running = false;
  }
}

AudioDevice::PlatformType DetectPlatform() {
  // TODO: implement platform detection
  return AudioDevice::PlatformType::kLinuxAlsa;
}

void PrintUsage(const char* program) {
  fprintf(stderr,
          "Usage: %s [options] <input_file>\n"
          "Options:\n"
          "  -f <format>      Audio format in hex (e.g. 0x1 for PCM16bit)\n"
          "  -s <sample_rate> Sample rate in Hz\n"
          "  -c <channels>    Number of channels\n"
          "  -p <platform>    Platform type (default: auto detect)\n",
          program);
}
}  // namespace

int main(int argc, char* argv[]) {
  int opt;
  uint32_t format = 0;
  int sample_rate = 0;
  int channels = 0;
  auto platform = AudioDevice::PlatformType::kDefault;

  while ((opt = getopt(argc, argv, "f:s:c:p:h")) != -1) {
    switch (opt) {
      case 'f':
        format = strtol(optarg, nullptr, 16);
        break;
      case 's':
        sample_rate = atoi(optarg);
        break;
      case 'c':
        channels = atoi(optarg);
        break;
      case 'p':
        // TODO: parse platform type
        break;
      case 'h':
      default:
        PrintUsage(argv[0]);
        return 1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Error: Input file is required\n");
    PrintUsage(argv[0]);
    return 1;
  }

  if (format == 0 || sample_rate == 0 || channels == 0) {
    fprintf(stderr, "Error: Format, sample rate and channels are required\n");
    PrintUsage(argv[0]);
    return 1;
  }

  const char* input_file = argv[optind];
  FILE* fp = fopen(input_file, "rb");
  if (!fp) {
    fprintf(stderr, "Error: Cannot open input file: %s\n", input_file);
    return 1;
  }

  // Set up signal handler
  signal(SIGINT, SignalHandler);

  // Create audio device
  if (platform == AudioDevice::PlatformType::kDefault) {
    platform = DetectPlatform();
  }

  auto audio_device = AudioDevice::CreateAudioDevice(platform);
  if (!audio_device) {
    fprintf(stderr, "Error: Failed to create audio device\n");
    fclose(fp);
    return 1;
  }

  // Create audio track
  AudioConfig config;
  config.format = static_cast<AudioFormat>(format);
  config.sample_rate = sample_rate;
  config.channel_layout = GuessChannelLayout(channels);

  auto audio_track = audio_device->CreateAudioTrack();
  if (!audio_track) {
    fprintf(stderr, "Error: Failed to create audio track\n");
    fclose(fp);
    return 1;
  }
  audio_track->Open(config);

  // Start playback
  const size_t buffer_size = 4096;  // Adjust buffer size as needed
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);

  audio_track->Start();

  while (g_running) {
    size_t bytes_read = fread(buffer.get(), 1, buffer_size, fp);
    if (bytes_read == 0) {
      break;  // End of file
    }

    size_t bytes_written = 0;
    while (bytes_written < bytes_read && g_running) {
      size_t written = audio_track->Write(buffer.get() + bytes_written,
                                        bytes_read - bytes_written);
      if (written == 0) {
        // Handle error or wait for buffer space
        usleep(10000);  // Sleep for 10ms
        continue;
      }
      bytes_written += written;
    }
  }

  // Cleanup
  audio_track->Stop();
  fclose(fp);

  return 0;
}
