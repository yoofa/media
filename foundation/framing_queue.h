/*
 * framing_queue.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef FRAMING_QUEUE_H
#define FRAMING_QUEUE_H

#include <memory>
#include <queue>
#include <vector>

#include "base/errors.h"
#include "media_frame.h"

namespace ave {
namespace media {

// FramingQueue is responsible for parsing raw bitstream data into
// complete frames (access units). It supports different codec types
// (H.264, AAC, etc.)
//
// For H.264: outputs complete Annex-B access units (all NALs for one picture,
//            with 0x00000001 start codes). Each popped frame is ready to be
//            passed directly to an H.264 decoder.
// For AAC:   outputs individual ADTS frames.
class FramingQueue {
 public:
  enum class CodecType {
    kH264,
    kAAC,
  };

  explicit FramingQueue(CodecType codec_type);
  ~FramingQueue();

  // Push raw data into the queue.
  // The queue will parse the data and extract complete frames.
  status_t PushData(const uint8_t* data, size_t size);

  // Get the next complete frame from the queue.
  // Returns nullptr if no complete frame is available.
  std::shared_ptr<MediaFrame> PopFrame();

  // Check if there are any frames available.
  bool HasFrame() const;

  // Get the number of frames in the queue.
  size_t FrameCount() const;

  // Clear all buffered data and frames.
  void Clear();

  // Flush any incomplete access unit as a final frame (call at end of stream).
  // After Flush(), HasFrame() will return true if there was pending data.
  void Flush();

 private:
  status_t ParseH264Frame();
  status_t ParseAACFrame();

  CodecType codec_type_;
  std::vector<uint8_t> buffer_;  // Accumulated input data
  std::vector<uint8_t> au_buf_;  // Current H.264 access unit being built
  bool au_has_vcl_ = false;      // Whether current AU contains a VCL NAL
  std::queue<std::shared_ptr<MediaFrame>> frames_;
};

}  // namespace media
}  // namespace ave

#endif /* !FRAMING_QUEUE_H */
