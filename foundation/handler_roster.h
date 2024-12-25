/*
 * HandlerRoster.h
 * Copyright (C) 2021 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_HANDLERROSTER_H
#define AVE_HANDLERROSTER_H

#include <memory>
#include <mutex>
#include <unordered_map>

#include "base/constructor_magic.h"
#include "looper.h"

namespace ave {
namespace media {

class HandlerRoster {
 public:
  HandlerRoster();
  virtual ~HandlerRoster() = default;

  Looper::handler_id registerHandler(const std::shared_ptr<Looper> &looper,
                                     const std::shared_ptr<Handler> &handler);

  void unregisterHandler(Looper::handler_id handler_id);

 private:
  struct HandlerInfo {
    std::weak_ptr<Looper> looper_;
    std::weak_ptr<Handler> handler_;
  };

  std::mutex mutex_;
  std::unordered_map<Looper::handler_id, HandlerInfo> handlers_;

  Looper::handler_id next_handler_id_;

  AVE_DISALLOW_COPY_AND_ASSIGN(HandlerRoster);
};

}  // namespace media
}  // namespace ave

#endif /* !AVE_HANDLERROSTER_H */
