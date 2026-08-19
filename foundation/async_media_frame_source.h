/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_FOUNDATION_ASYNC_MEDIA_FRAME_SOURCE_H_
#define MEDIA_FOUNDATION_ASYNC_MEDIA_FRAME_SOURCE_H_

#include <memory>

#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_factory.h"
#include "media/foundation/media_source_base.h"

namespace ave::media {

// Dispatches frames away from the producer callback. This keeps a slow sink
// from blocking a codec or capture callback. Sinks must be removed before they
// are destroyed.
class AsyncMediaFrameSource : public MediaFrameSource {
 public:
  explicit AsyncMediaFrameSource(base::TaskRunnerFactory* task_runner_factory);
  ~AsyncMediaFrameSource() override;

  void AddOrUpdateSink(MediaSinkInterface<std::shared_ptr<MediaFrame>>* sink,
                       const MediaSinkWants& wants) override;
  void RemoveSink(
      MediaSinkInterface<std::shared_ptr<MediaFrame>>* sink) override;

 protected:
  void DeliverFrame(const std::shared_ptr<MediaFrame>& frame);

 private:
  std::unique_ptr<base::TaskRunnerFactory> owned_task_runner_factory_;
  std::unique_ptr<base::TaskRunner> dispatch_runner_;
};

}  // namespace ave::media

#endif  // MEDIA_FOUNDATION_ASYNC_MEDIA_FRAME_SOURCE_H_
