/*
 * test_decode.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <getopt.h>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/task_util/default_task_runner_factory.h"
#include "dummy_codec_factory.h"
#include "media/codec/codec_factory.h"
#include "media/codec/ffmpeg/ffmpeg_codec_factory.h"
#include "test_codec_helper.h"

using ave::status_t;
using ave::media::CHANNEL_LAYOUT_STEREO;
using ave::media::CodecFactory;
using ave::media::MediaFormat;
using ave::media::test::DummyCodecFactory;
using ave::media::test::TestCodecRunner;

void PrintUsage(const char* program) {
  fprintf(
      stderr,
      "Usage: %s [options] <input_file> <output_file>\n"
      "Options:\n"
      "  -c <codec>      Codec factory to use (ffmpeg, nmediacodec. dummy)\n"
      "  -m <mime>       MIME type (audio/aac, video/h264, etc)\n"
      "  -w <width>      Video width\n"
      "  -h <height>     Video height\n"
      "  -r <rate>       Sample rate for audio\n"
      "  -n <channels>   Number of channels for audio\n",
      program);
}

int main(int argc, char* argv[]) {
  std::string codec_platform_name = "ffmpeg";
  std::string mime;
  int width = 0;
  int height = 0;
  int sample_rate = 0;
  int channels = 0;

  int opt{};
  while ((opt = getopt(argc, argv, "c:m:w:h:r:n:")) != -1) {
    switch (opt) {
      case 'c':
        codec_platform_name = optarg;
        break;
      case 'm':
        mime = optarg;
        break;
      case 'w':
        width = atoi(optarg);
        break;
      case 'h':
        height = atoi(optarg);
        break;
      case 'r':
        sample_rate = atoi(optarg);
        break;
      case 'n':
        channels = atoi(optarg);
        break;
      default:
        PrintUsage(argv[0]);
        return 1;
    }
  }

  if (optind + 2 > argc) {
    PrintUsage(argv[0]);
    return 1;
  }

  const char* input_file = argv[optind];
  const char* output_file = argv[optind + 1];
  const ave::media::CodecPlatform codec_platform =
      ave::media::test::NameToCodecPlatform(codec_platform_name);

  // Create codec factory
  std::shared_ptr<CodecFactory> factory;

  switch (codec_platform) {
#if defined(AVE_FFMPEG_CODEC)
    case ave::media::CodecPlatform::kFFmpeg:
      factory = std::make_shared<ave::media::FFmpegCodecFactory>();
      break;
#endif

#if defined(AVE_ANDROID_MEDIA_CODEC)
    case ave::media::CodecPlatform::kAndroidNdkMediaCodec:
      // TODO: Implement NMediaCodecFactory
      break;
#endif

    case ave::media::CodecPlatform::kDummy:
      factory = std::make_shared<DummyCodecFactory>();
      break;
    default:
      AVE_LOG(LS_ERROR) << "Unsupported codec platform: "
                        << codec_platform_name;
      return 1;
  }

  auto task_runner_factory = ave::base::CreateDefaultTaskRunnerFactory();

  // Create media format
  auto format = MediaFormat::CreatePtr();
  if (mime.find("video") != std::string::npos) {
    format->SetWidth(width);
    format->SetHeight(height);
  } else if (mime.find("audio") != std::string::npos) {
    format->SetSampleRate(sample_rate);
    format->SetChannelLayout(CHANNEL_LAYOUT_STEREO);
  }

  // Open input/output files
  FILE* in_fp = fopen(input_file, "rb");
  FILE* out_fp = fopen(output_file, "wb");
  if (!in_fp || !out_fp) {
    AVE_LOG(LS_ERROR) << "Failed to open files";
    return 1;
  }

  // Create input/output callbacks
  auto input_cb = [in_fp](uint8_t* data, size_t size) -> ssize_t {
    return fread(data, 1, size, in_fp);
  };

  auto output_cb = [out_fp](const uint8_t* data, size_t size) -> ssize_t {
    return fwrite(data, 1, size, out_fp);
  };

  // Create test runner
  TestCodecRunner runner(factory.get(), task_runner_factory.get(), input_cb,
                         output_cb);
  status_t err = runner.Init(mime, false, format);
  if (err != ave::OK) {
    AVE_LOG(LS_ERROR) << "Failed to initialize codec runner: " << err;
    return 1;
  }

  // Start processing
  err = runner.Start();
  if (err != ave::OK) {
    AVE_LOG(LS_ERROR) << "Failed to start codec runner: " << err;
    return 1;
  }

  // Wait for completion

  fclose(in_fp);
  fclose(out_fp);

  return 0;
}
