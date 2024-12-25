/*
 * HandlerRoster.cc
 * Copyright (C) 2021 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "handler_roster.h"

#include <memory>
#include <mutex>

#include "handler.h"
#include "looper.h"
#include "message.h"

namespace ave {
namespace media {

HandlerRoster::HandlerRoster() : next_handler_id_(static_cast<int32_t>(1)) {}

Looper::handler_id
HandlerRoster::registerHandler(const std::shared_ptr<Looper> &looper,
                               const std::shared_ptr<Handler> &handler) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (handler->id() != 0) {
    return static_cast<int32_t>(-1);
  }

  HandlerInfo info;
  info.looper_ = looper;
  info.handler_ = handler;
  auto handler_id = static_cast<Looper::handler_id>(
      static_cast<int32_t>(next_handler_id_) + 1);
  handlers_.emplace(handler_id, info);

  handler->setId(handler_id, looper);

  return handler_id;
}

void HandlerRoster::unregisterHandler(Looper::handler_id handler_id) {
  std::lock_guard<std::mutex> guard(mutex_);

  auto it = handlers_.find(handler_id);
  if (it != handlers_.end()) {
    HandlerInfo info = it->second;
    std::shared_ptr<Handler> handler = info.handler_.lock();
    if (handler != nullptr) {
      handler->setId(static_cast<int32_t>(0), std::weak_ptr<Looper>());
    }

    handlers_.erase(it);
  }
}

} // namespace media
} // namespace ave
