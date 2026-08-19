/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/foundation/async_media_frame_source.h"

#include "base/task_util/default_task_runner_factory.h"

namespace ave::media {

AsyncMediaFrameSource::AsyncMediaFrameSource(
    base::TaskRunnerFactory* task_runner_factory) {
  if (task_runner_factory == nullptr) {
    owned_task_runner_factory_ = base::CreateDefaultTaskRunnerFactory();
    task_runner_factory = owned_task_runner_factory_.get();
  }
  dispatch_runner_ =
      std::make_unique<base::TaskRunner>(task_runner_factory->CreateTaskRunner(
          "MediaFrameDispatch", base::TaskRunnerFactory::Priority::NORMAL));
}

AsyncMediaFrameSource::~AsyncMediaFrameSource() = default;

void AsyncMediaFrameSource::AddOrUpdateSink(
    MediaSinkInterface<std::shared_ptr<MediaFrame>>* sink,
    const MediaSinkWants& wants) {
  if (dispatch_runner_->IsCurrent()) {
    MediaFrameSource::AddOrUpdateSink(sink, wants);
    return;
  }
  dispatch_runner_->PostTaskAndWait([this, sink, wants]() {
    MediaFrameSource::AddOrUpdateSink(sink, wants);
  });
}

void AsyncMediaFrameSource::RemoveSink(
    MediaSinkInterface<std::shared_ptr<MediaFrame>>* sink) {
  if (dispatch_runner_->IsCurrent()) {
    MediaFrameSource::RemoveSink(sink);
    return;
  }
  dispatch_runner_->PostTaskAndWait(
      [this, sink]() { MediaFrameSource::RemoveSink(sink); });
}

void AsyncMediaFrameSource::DeliverFrame(
    const std::shared_ptr<MediaFrame>& frame) {
  if (frame == nullptr) {
    return;
  }
  dispatch_runner_->PostTask([this, frame]() {
    for (const SinkPair& sink_pair : sink_pairs()) {
      sink_pair.sink->OnFrame(frame);
    }
  });
}

}  // namespace ave::media
