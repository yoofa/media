/*
 * media_source_sink_interface.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_SOURCE_SINK_INTERFACE_H
#define MEDIA_SOURCE_SINK_INTERFACE_H

#include <optional>
#include <vector>

namespace ave {
namespace media {

struct MediaSinkWants {
  struct FrameSize {
    int width = 0;
    int height = 0;
  };
  MediaSinkWants() = default;
  MediaSinkWants(const MediaSinkWants&) = default;
  ~MediaSinkWants() = default;

  /********** Video **********/

  // Tells the source whether the sink wants frames with rotation applied.
  // By default, any rotation must be applied by the sink.
  bool rotation_applied = false;

  // Tells the source that the sink only wants black frames.
  bool black_frames = false;

  // Tells the source the maximum number of pixels the sink wants.
  int max_pixel_count = std::numeric_limits<int>::max();
  // Tells the source the desired number of pixels the sinks wants. This will
  // typically be used when stepping the resolution up again when conditions
  // have improved after an earlier downgrade. The source should select the
  // closest resolution to this pixel count, but if max_pixel_count is set, it
  // still sets the absolute upper bound.
  std::optional<int> target_pixel_count;
  // Tells the source the maximum framerate the sink wants.
  int max_framerate_fps = std::numeric_limits<int>::max();

  // Tells the source that the sink wants width and height of the video frames
  // to be divisible by `resolution_alignment`.
  // For example: With I420, this value would be a multiple of 2.
  // Note that this field is unrelated to any horizontal or vertical stride
  // requirements the encoder has on the incoming video frame buffers.
  int resolution_alignment = 1;

  // The resolutions that sink is configured to consume. If the sink is an
  // encoder this is what the encoder is configured to encode. In singlecast we
  // only encode one resolution, but in simulcast and SVC this can mean multiple
  // resolutions per frame.
  //
  // The sink is always configured to consume a subset of the
  // webrtc::VideoFrame's resolution. In the case of encoding, we usually encode
  // at webrtc::VideoFrame's resolution but this may not always be the case due
  // to scaleResolutionDownBy or turning off simulcast or SVC layers.
  //
  // For example, we may capture at 720p and due to adaptation (e.g. applying
  // `max_pixel_count` constraints) create webrtc::VideoFrames of size 480p, but
  // if we do scaleResolutionDownBy:2 then the only resolution we end up
  // encoding is 240p. In this case we still need to provide webrtc::VideoFrames
  // of size 480p but we can optimize internal buffers for 240p, avoiding
  // downsampling to 480p if possible.
  //
  // Note that the `resolutions` can change while frames are in flight and
  // should only be used as a hint when constructing the webrtc::VideoFrame.
  std::vector<FrameSize> resolutions;
};

inline bool operator==(const MediaSinkWants::FrameSize& a,
                       const MediaSinkWants::FrameSize& b) {
  return a.width == b.width && a.height == b.height;
}

template <typename MediaFrameT>
class MediaSinkInterface {
 public:
  virtual ~MediaSinkInterface() = default;

  virtual void OnFrame(const MediaFrameT& frame) = 0;
};

template <typename MediaFrameT>
class MediaSourceInterface {
 public:
  virtual ~MediaSourceInterface() = default;

  virtual void AddOrUpdateSink(MediaSinkInterface<MediaFrameT>* sink,
                               const MediaSinkWants& wants) = 0;
  virtual void RemoveSink(MediaSinkInterface<MediaFrameT>* sink) = 0;
};

}  // namespace media
}  // namespace ave

#endif /* !MEDIA_SOURCE_SINK_INTERFACE_H */
