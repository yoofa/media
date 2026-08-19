/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/foundation/async_media_frame_source.h"
#include "media/foundation/media_sink_base.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "test/gtest.h"

namespace ave::media {
namespace {

class TestAsyncMediaFrameSource final : public AsyncMediaFrameSource {
 public:
  TestAsyncMediaFrameSource() : AsyncMediaFrameSource(nullptr) {}

  void Push(const std::shared_ptr<MediaFrame>& frame) { DeliverFrame(frame); }
};

class BlockingSink final : public MediaFrameSink {
 public:
  void OnFrame(const std::shared_ptr<MediaFrame>& /*frame*/) override {
    std::unique_lock lock(mutex_);
    entered_ = true;
    condition_.notify_all();
    condition_.wait(lock, [this]() { return released_; });
  }

  void WaitUntilEntered() {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this]() { return entered_; });
  }

  void Release() {
    std::scoped_lock lock(mutex_);
    released_ = true;
    condition_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  bool entered_ = false;
  bool released_ = false;
};

class CountingSink final : public MediaFrameSink {
 public:
  void OnFrame(const std::shared_ptr<MediaFrame>& /*frame*/) override {
    std::scoped_lock lock(mutex_);
    ++count_;
    condition_.notify_all();
  }

  void WaitForCount(size_t expected) {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this, expected]() { return count_ >= expected; });
  }

  size_t count() const {
    std::scoped_lock lock(mutex_);
    return count_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  size_t count_ = 0;
};

TEST(AsyncMediaFrameSourceTest, FansOutWithoutBlockingProducer) {
  TestAsyncMediaFrameSource source;
  BlockingSink slow_sink;
  CountingSink second_sink;
  source.AddOrUpdateSink(&slow_sink, {});
  source.AddOrUpdateSink(&second_sink, {});

  std::atomic<bool> producer_returned = false;
  std::thread producer([&source, &producer_returned]() {
    source.Push(MediaFrame::CreateShared(8, MediaType::AUDIO));
    producer_returned.store(true);
  });
  slow_sink.WaitUntilEntered();
  EXPECT_TRUE(producer_returned.load());
  slow_sink.Release();
  producer.join();
  second_sink.WaitForCount(1);
  EXPECT_EQ(second_sink.count(), 1U);

  source.RemoveSink(&slow_sink);
  source.RemoveSink(&second_sink);
}

}  // namespace
}  // namespace ave::media
