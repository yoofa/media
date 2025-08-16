/*
 * media_source_base.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_SOURCE_BASE_H
#define MEDIA_SOURCE_BASE_H

#include "media_frame.h"
#include "media_source_sink_interface.h"

namespace ave {
namespace media {

template <typename MediaFrameT>
class MediaSourceBase : public MediaSourceInterface<MediaFrameT> {
 public:
  void AddOrUpdateSink(MediaSinkInterface<MediaFrameT>* sink,
                       const MediaSinkWants& wants) override {
    AVE_DCHECK(sink != nullptr);
    SinkPair* sink_pair = FindSinkPair(sink);
    if (sink_pair) {
      sink_pair->wants = wants;
      return;
    }
    sinks_.push_back(SinkPair(sink, wants));
  }

  void RemoveSink(MediaSinkInterface<MediaFrameT>* sink) override {
    AVE_DCHECK(FindSinkPair(sink));
    sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(),
                                [sink](const SinkPair& sink_pair) {
                                  return sink_pair.sink == sink;
                                }),
                 sinks_.end());
  }

  struct SinkPair {
    SinkPair(MediaSinkInterface<MediaFrameT>* sink, MediaSinkWants wants)
        : sink(sink), wants(wants) {}
    MediaSinkInterface<MediaFrameT>* sink;
    MediaSinkWants wants;
  };

  SinkPair* FindSinkPair(const MediaSinkInterface<MediaFrameT>* sink) {
    AVE_DCHECK(sink != nullptr);

    auto sink_pair_it = std::find_if(
        sinks_.begin(), sinks_.end(),
        [sink](const SinkPair& sink_pair) { return sink_pair.sink == sink; });

    if (sink_pair_it != sinks_.end()) {
      return &(*sink_pair_it);
    }
    return nullptr;
  }

  const std::vector<SinkPair>& sink_pairs() const { return sinks_; }

 private:
  std::vector<SinkPair> sinks_;
};

class MediaFrameSource : public MediaSourceBase<std::shared_ptr<MediaFrame>> {
 public:
  ~MediaFrameSource() override = default;
  void AddOrUpdateSink(MediaSinkInterface<std::shared_ptr<MediaFrame>>* sink,
                       const MediaSinkWants& wants) override {
    MediaSourceBase<std::shared_ptr<MediaFrame>>::AddOrUpdateSink(sink, wants);
  }

  void RemoveSink(
      MediaSinkInterface<std::shared_ptr<MediaFrame>>* sink) override {
    MediaSourceBase<std::shared_ptr<MediaFrame>>::RemoveSink(sink);
  }
};

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_SOURCE_BASE_H */
