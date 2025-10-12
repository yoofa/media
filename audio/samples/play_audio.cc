/*
 * play_audio.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <getopt.h>
#include <unistd.h>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "base/task_util/repeating_task.h"
#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_stdlib.h"

#include "base/logging.h"
#include "media/audio/audio.h"
#include "media/audio/audio_device.h"
#include "media/audio/audio_format.h"
#include "media/audio/audio_track.h"
#include "media/audio/channel_layout.h"

using ave::base::RepeatingTaskHandle;
using ave::base::TaskRunner;
using ave::media::AudioConfig;
using ave::media::AudioDevice;
using ave::media::AudioFormat;
using ave::media::AudioTrack;
using ave::media::GuessChannelLayout;

namespace {
volatile bool g_running = true;

void SignalHandler(int signal) {
  if (signal == SIGINT) {
    g_running = false;
  }
}

AudioDevice::PlatformType DetectPlatform() {
  // TODO: implement platform detection
#if defined(AVE_ANDROID)
  return AudioDevice::PlatformType::kAndroidAAudio;
#elif defined(AVE_LINUX)
  return AudioDevice::PlatformType::kLinuxAlsa;
#endif
}

void PrintUsage(const char* program) {
  fprintf(
      stderr,
      "Usage: %s [options] <input_file>\n"
      "Options:\n"
      "  -f <format>      Audio format in hex (e.g. 0x1 for PCM16bit)\n"
      "  -s <sample_rate> Sample rate in Hz\n"
      "  -c <channels>    Number of channels\n"
      "  -p <platform>    Platform type (default: auto detect)\n"
      "  -m <mode>        Playback mode: sink or callback (default: sink)\n",
      program);
}
}  // namespace

class SinkPlayer {
 public:
  SinkPlayer(std::shared_ptr<AudioTrack> track,
             FILE* file,
             const AudioConfig& config,
             size_t io_buffer_size)
      : track_(std::move(track)),
        file_(file),
        config_(config),
        io_buffer_size_(io_buffer_size),
        buffer_(new uint8_t[io_buffer_size]),
        task_runner_(std::make_unique<TaskRunner>(
            ave::base::CreateTaskRunnerStdlibFactory()->CreateTaskRunner(
                "sink_player",
                ave::base::TaskRunnerFactory::Priority::HIGH))) {}

  ~SinkPlayer() { Stop(); }

  ave::status_t Start() {
    if (started_.exchange(true)) {
      return ave::ALREADY_EXISTS;
    }
    if (track_->Open(config_) != ave::OK) {
      return ave::INVALID_OPERATION;
    }
    if (track_->Start() != ave::OK) {
      return ave::INVALID_OPERATION;
    }

    running_.store(true);
    repeating_ = RepeatingTaskHandle::Start(task_runner_->Get(), [this]() {
      if (!running_.load()) {
        repeating_.Stop();
        return uint64_t{0};
      }

      size_t bytes_read = fread(buffer_.get(), 1, io_buffer_size_, file_);
      if (bytes_read == 0) {
        // should stop repeating task
        running_.store(false);
        return uint64_t{0};
      }

      size_t offset = 0;
      while (offset < bytes_read && running_.load()) {
        ssize_t written =
            track_->Write(buffer_.get() + offset, bytes_read - offset, true);
        if (written <= 0) {
          return uint64_t{1000};
        }
        offset += static_cast<size_t>(written);
      }

      return uint64_t{0};
    });

    return ave::OK;
  }

  void Pause() {
    running_.store(false);
    track_->Pause();
  }

  void Resume() {
    if (!started_.load()) {
      return;
    }
    track_->Start();
    running_.store(true);
  }

  void Stop() {
    if (!started_.load()) {
      return;
    }
    running_.store(false);
    repeating_.Stop();
    track_->Stop();
    track_->Close();
    started_.store(false);
  }

 private:
  std::shared_ptr<AudioTrack> track_;
  FILE* file_;
  AudioConfig config_{};
  size_t io_buffer_size_{};
  std::unique_ptr<uint8_t[]> buffer_;

  std::unique_ptr<TaskRunner> task_runner_;
  RepeatingTaskHandle repeating_;
  std::atomic<bool> running_{false};
  std::atomic<bool> started_{false};
};

class CallbackPlayer {
 public:
  CallbackPlayer(std::shared_ptr<AudioTrack> track,
                 FILE* file,
                 const AudioConfig& config,
                 size_t io_buffer_size)
      : track_(std::move(track)), file_(file), config_(config) {
    (void)io_buffer_size;  // Unused in callback mode
  }

  ~CallbackPlayer() { Stop(); }

  ave::status_t Start() {
    if (started_.exchange(true)) {
      return ave::ALREADY_EXISTS;
    }

    auto callback = [this](AudioTrack* audio_track, void* buffer, size_t size,
                           void* cookie, AudioTrack::cb_event_t event) {
      this->OnAudioCallback(audio_track, buffer, size, cookie, event);
    };

    if (track_->Open(config_, callback, this) != ave::OK) {
      return ave::INVALID_OPERATION;
    }
    if (track_->Start() != ave::OK) {
      return ave::INVALID_OPERATION;
    }

    return ave::OK;
  }

  void Stop() {
    if (!started_.load()) {
      return;
    }
    track_->Stop();
    track_->Close();
    started_.store(false);
  }

 private:
  void OnAudioCallback(AudioTrack* audio_track,
                       void* buffer,
                       size_t size,
                       void* cookie,
                       AudioTrack::cb_event_t event) {
    if (event == AudioTrack::CB_EVENT_FILL_BUFFER) {
      size_t bytes_read = fread(buffer, 1, size, file_);
      if (bytes_read < size) {
        // Pad with silence if reached end of file
        memset(static_cast<uint8_t*>(buffer) + bytes_read, 0,
               size - bytes_read);
      }
    } else if (event == AudioTrack::CB_EVENT_STREAM_END) {
      AVE_LOG(LS_INFO) << "Stream end event received";
    } else if (event == AudioTrack::CB_EVENT_TEAR_DOWN) {
      AVE_LOG(LS_INFO) << "Tear down event received";
    }
  }

  std::shared_ptr<AudioTrack> track_;
  FILE* file_;
  AudioConfig config_{};
  std::atomic<bool> started_{false};
};

int main(int argc, char* argv[]) {
  int opt = 0;
  uint32_t format = 0;
  int sample_rate = 0;
  int channels = 0;
  auto platform = AudioDevice::PlatformType::kDefault;
  std::string mode = "sink";

  while ((opt = getopt(argc, argv, "f:s:c:p:m:h")) != -1) {
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
      case 'p': {
        if (strcmp(optarg, "alsa") == 0) {
          platform = AudioDevice::PlatformType::kLinuxAlsa;
        } else if (strcmp(optarg, "pulse") == 0) {
          platform = AudioDevice::PlatformType::kLinuxPulse;
        } else if (strcmp(optarg, "aaudio") == 0) {
          platform = AudioDevice::PlatformType::kAndroidAAudio;
        } else if (strcmp(optarg, "opensles") == 0) {
          platform = AudioDevice::PlatformType::kAndroidOpenSLES;
        } else {
          fprintf(stderr, "Unknown platform: %s\n", optarg);
          PrintUsage(argv[0]);
          return 1;
        }
        break;
      }
      case 'm':
        mode = optarg;
        if (mode != "sink" && mode != "callback") {
          fprintf(stderr, "Unknown mode: %s\n", optarg);
          PrintUsage(argv[0]);
          return 1;
        }
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
  if (audio_device->Init() != ave::OK) {
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

  const size_t buffer_size = 4096;

  if (mode == "sink") {
    SinkPlayer player(audio_track, fp, config, buffer_size);
    if (player.Start() != ave::OK) {
      fprintf(stderr, "Error: Failed to start sink player\n");
      fclose(fp);
      return 1;
    }

    while (g_running) {
      usleep(1000 * 100);
    }

    player.Stop();
  } else {  // callback mode
    CallbackPlayer player(audio_track, fp, config, buffer_size);
    if (player.Start() != ave::OK) {
      fprintf(stderr, "Error: Failed to start callback player\n");
      fclose(fp);
      return 1;
    }

    while (g_running) {
      usleep(1000 * 100);
    }

    player.Stop();
  }

  fclose(fp);

  return 0;
}
