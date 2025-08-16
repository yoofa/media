/*
 * media_source_test.cc
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */
#include <memory>
#include "base/errors.h"

#include "media/foundation/media_meta.h"
#include "media/foundation/media_source.h"
#include "media/foundation/media_source_sink_interface.h"

using ave::status_t;
using ave::media::MediaSource;
namespace ave {
namespace media {

class TestSource : public MediaSource {
 public:
  TestSource() = default;
  ~TestSource() override = default;

  status_t Read(std::shared_ptr<MediaFrame>& packet,
                const ReadOptions* options) override {
    return ERROR_UNSUPPORTED;
  }

  status_t Start(std::shared_ptr<Message> params) override {
    return ERROR_UNSUPPORTED;
  }

  status_t Stop() override { return ERROR_UNSUPPORTED; }
  status_t Pause() override { return ERROR_UNSUPPORTED; }
  std::shared_ptr<MediaMeta> GetFormat() override { return nullptr; }

  status_t SetBuffers(
      const std::vector<std::shared_ptr<MediaFrame>>& buffers) override {
    return ERROR_UNSUPPORTED;
  }
  status_t ReadMultiple(std::vector<std::shared_ptr<MediaFrame>>& packets,
                        size_t count,
                        const ReadOptions* options) override {
    return ERROR_UNSUPPORTED;
  }
};
}  // namespace media
}  // namespace ave

int main(int argc, char* argv[]) {
  ave::media::TestSource test_source;
  test_source.AddOrUpdateSink(nullptr, ave::media::MediaSinkWants());
  return 0;
}
