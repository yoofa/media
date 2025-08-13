/*
 * test_codec_helper.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_CODEC_TEST_CODEC_HELPER_H_
#define MEDIA_CODEC_TEST_CODEC_HELPER_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_factory.h"
#include "media/codec/codec.h"
#include "media/codec/codec_factory.h"

namespace ave {
namespace media {
namespace test {

using ave::media::CodecPlatform;

CodecPlatform NameToCodecPlatform(const std::string& name);

using InputDataCallback = std::function<ssize_t(uint8_t* data, size_t size)>;
using OutputDataCallback =
    std::function<ssize_t(const uint8_t* data, size_t size)>;

class TestCodecRunner : public CodecCallback {
 public:
  TestCodecRunner(CodecFactory* factory,
                  base::TaskRunnerFactory* task_runner_factory,
                  InputDataCallback input_cb,
                  OutputDataCallback output_cb);
  ~TestCodecRunner() override;

  status_t Init(const std::string& mime,
                bool is_encoder,
                const std::shared_ptr<MediaMeta>& format);
  status_t Start();
  status_t Stop();
  bool IsRunning() const { return running_; }

  // Wait for processing to complete
  status_t WaitForCompletion();

  // CodecCallback implementation
  void OnInputBufferAvailable(size_t index) override;
  void OnOutputBufferAvailable(size_t index) override;
  void OnOutputFormatChanged(const std::shared_ptr<MediaMeta>& format) override;
  void OnError(status_t error) override;
  void OnFrameRendered(std::shared_ptr<Message> notify) override;

 private:
  void ProcessInput(size_t index);
  void ProcessOutput(size_t index);
  void HandleError(status_t error);
  void CheckCompletion();

  CodecFactory* factory_;
  std::shared_ptr<Codec> codec_;
  InputDataCallback input_cb_;
  OutputDataCallback output_cb_;
  bool is_encoder_;
  std::shared_ptr<MediaMeta> format_;

  std::unique_ptr<base::TaskRunner> task_runner_;
  bool running_ = false;
  bool error_ = false;
  bool eos_sent_ = false;

  // Completion tracking
  std::mutex completion_lock_;
  std::condition_variable completion_cv_;
  bool completed_ = false;
  bool has_pending_input_ = false;
  bool has_pending_output_ = false;
};

}  // namespace test
}  // namespace media
}  // namespace ave

#endif  // MEDIA_CODEC_TEST_CODEC_HELPER_H_
