/*
 * record_audio.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <getopt.h>
#include <unistd.h>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <memory>

#include "base/task_util/repeating_task.h"
#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_stdlib.h"

#include "base/logging.h"
#include "media/audio/audio.h"
#include "media/audio/audio_device.h"
#include "media/audio/audio_format.h"
#include "media/audio/audio_record.h"
#include "media/audio/channel_layout.h"

using ave::base::RepeatingTaskHandle;
using ave::base::TaskRunner;
using ave::media::AudioConfig;
using ave::media::AudioDevice;
using ave::media::AudioFormat;
using ave::media::AudioRecord;
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
  fprintf(stderr,
          "Usage: %s [options] <output_file>\n"
          "Options:\n"
          "  -f <format>      Audio format in hex (e.g. 0x1 for PCM16bit)\n"
          "  -s <sample_rate> Sample rate in Hz\n"
          "  -c <channels>    Number of channels\n"
          "  -p <platform>    Platform type (default: auto detect)\n",
          program);
}
}  // namespace

class SourceRecorder {
 public:
  SourceRecorder(std::shared_ptr<AudioRecord> record,
                 FILE* file,
                 const AudioConfig& config,
                 size_t io_buffer_size)
      : record_(std::move(record)),
        file_(file),
        config_(config),
        io_buffer_size_(io_buffer_size),
        buffer_(new uint8_t[io_buffer_size]),
        task_runner_(std::make_unique<TaskRunner>(
            ave::base::CreateTaskRunnerStdlibFactory()->CreateTaskRunner(
                "source_recorder",
                ave::base::TaskRunnerFactory::Priority::HIGH))) {}

  ~SourceRecorder() { Stop(); }

  ave::status_t Start() {
    if (started_.exchange(true)) {
      return ave::ALREADY_EXISTS;
    }
    if (record_->Open(config_) != ave::OK) {
      return ave::INVALID_OPERATION;
    }
    if (record_->Start() != ave::OK) {
      return ave::INVALID_OPERATION;
    }

    running_.store(true);
    repeating_ = RepeatingTaskHandle::Start(task_runner_->Get(), [this]() {
      if (!running_.load()) {
        repeating_.Stop();
        return uint64_t{0};
      }

      ssize_t read = record_->Read(buffer_.get(), io_buffer_size_, true);
      if (read <= 0) {
        return uint64_t{1000};
      }
      size_t written =
          fwrite(buffer_.get(), 1, static_cast<size_t>(read), file_);
      (void)written;

      return uint64_t{0};
    });

    return ave::OK;
  }

  void Pause() {
    running_.store(false);
    record_->Pause();
  }

  void Resume() {
    if (!started_.load()) {
      return;
    }
    record_->Start();
    running_.store(true);
  }

  void Stop() {
    if (!started_.load()) {
      return;
    }
    running_.store(false);
    repeating_.Stop();
    record_->Stop();
    record_->Close();
    started_.store(false);
  }

 private:
  std::shared_ptr<AudioRecord> record_;
  FILE* file_;
  AudioConfig config_{};
  size_t io_buffer_size_{};
  std::unique_ptr<uint8_t[]> buffer_;

  std::unique_ptr<TaskRunner> task_runner_;
  RepeatingTaskHandle repeating_{};
  std::atomic<bool> running_{false};
  std::atomic<bool> started_{false};
};

int main(int argc, char* argv[]) {
  int opt = 0;
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
      case 'h':
      default:
        PrintUsage(argv[0]);
        return 1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Error: Output file is required\n");
    PrintUsage(argv[0]);
    return 1;
  }

  if (format == 0 || sample_rate == 0 || channels == 0) {
    fprintf(stderr, "Error: Format, sample rate and channels are required\n");
    PrintUsage(argv[0]);
    return 1;
  }

  const char* output_file = argv[optind];
  FILE* fp = fopen(output_file, "wb");
  if (!fp) {
    fprintf(stderr, "Error: Cannot open output file: %s\n", output_file);
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
    fclose(fp);
    return 1;
  }

  // Create audio record
  AudioConfig config;
  config.format = static_cast<AudioFormat>(format);
  config.sample_rate = sample_rate;
  config.channel_layout = GuessChannelLayout(channels);

  auto audio_record = audio_device->CreateAudioRecord();
  if (!audio_record) {
    fprintf(stderr, "Error: Failed to create audio record\n");
    fclose(fp);
    return 1;
  }

  const size_t buffer_size = 4096;
  SourceRecorder recorder(audio_record, fp, config, buffer_size);
  if (recorder.Start() != ave::OK) {
    fprintf(stderr, "Error: Failed to start recorder\n");
    fclose(fp);
    return 1;
  }

  while (g_running) {
    usleep(1000 * 100);
  }

  recorder.Stop();
  fclose(fp);

  return 0;
}
