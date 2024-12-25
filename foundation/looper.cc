/*
 * Looper.cc
 * Copyright (C) 2021 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "looper.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "base/attributes.h"
#include "base/count_down_latch.h"
#include "handler_roster.h"
#include "message.h"

namespace ave {
namespace media {

HandlerRoster gRoster;

Looper::Looper()
    : priority_(static_cast<int32_t>(0)), thread_(nullptr), looping_(false),
      start_latch_(1), stopped_(false) {}

Looper::~Looper() { stop(); }

void Looper::setName(std::string name) { name_ = name; }

Looper::handler_id
Looper::registerHandler(const std::shared_ptr<Handler> &handler) {
  return gRoster.registerHandler(shared_from_this(), handler);
}

void Looper::unregisterHandler(handler_id handler_id) {
  gRoster.unregisterHandler(handler_id);
}

int32_t Looper::start(int32_t priority AVE_MAYBE_UNUSED) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (thread_ != nullptr) {
    return static_cast<int32_t>(-1);
  }

  thread_ = std::make_unique<std::thread>(&Looper::loop, this);
  looping_ = true;
  start_latch_.Wait();
  return static_cast<int32_t>(0);
}

int32_t Looper::stop() {
  // TODO(youfa) support stop in loop thread.
  {
    std::lock_guard<std::mutex> guard(mutex_);
    stopped_ = true;
    looping_ = false;
    condition_.notify_all();
  }
  if (thread_ != nullptr) {
    thread_->join();
    (void)thread_.release();
  }
  return static_cast<int32_t>(0);
}

void Looper::post(const std::shared_ptr<Message> &message, int64_t delay_us) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (stopped_) {
    return;
  }
  int64_t whenUs = 0;
  if (delay_us > 0) {
    int64_t nowUs = getNowUs();
    whenUs = (delay_us > (std::numeric_limits<int64_t>::max() - nowUs)
                  ? std::numeric_limits<int64_t>::max()
                  : (nowUs + delay_us));
  } else {
    whenUs = getNowUs();
  }

  std::unique_ptr<Event> event = std::make_unique<Event>();
  event->when_us_ = whenUs;
  event->message_ = std::move(message);
  event_queue_.push(std::move(event));
  condition_.notify_all();
}

void Looper::loop() {
  start_latch_.CountDown();
  while (keepRunning()) {
    std::shared_ptr<Message> message;
    {
      std::unique_lock<std::mutex> l(mutex_);

      if (event_queue_.empty()) {
        condition_.wait(l);
        continue;
      }

      const auto &event = event_queue_.top();
      int64_t nowUs = getNowUs();

      if (event->when_us_ > nowUs) {
        int64_t delay_us = event->when_us_ - nowUs;
        if (delay_us > std::numeric_limits<int64_t>::max()) {
          delay_us = std::numeric_limits<int64_t>::max();
        }

        condition_.wait_for(l, std::chrono::microseconds(delay_us));
        continue;
      }

      message = std::move(event->message_);
      event_queue_.pop();
    }
    message->deliver();
    message.reset();
  }
}

bool Looper::keepRunning() {
  std::lock_guard<std::mutex> l(mutex_);
  return looping_ || !event_queue_.empty();
}

std::shared_ptr<ReplyToken> Looper::createReplyToken() {
  return std::make_shared<ReplyToken>(shared_from_this());
}

status_t Looper::awaitResponse(const std::shared_ptr<ReplyToken> &replyToken,
                               std::shared_ptr<Message> &response) {
  std::unique_lock<std::mutex> guard(mutex_);
  // AVE_CHECK(replyToken != NULL)
  while (!replyToken->getReply(response)) {
    replies_condition_.wait(guard);
  }

  return 0;
}

status_t Looper::postReply(const std::shared_ptr<ReplyToken> &replyToken,
                           const std::shared_ptr<Message> &reply) {
  std::lock_guard<std::mutex> guard(mutex_);
  status_t err = replyToken->setReply(reply);
  if (err == 0) {
    replies_condition_.notify_all();
  }
  return err;
}

} // namespace media
} // namespace ave
