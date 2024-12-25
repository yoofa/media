/*
 * Handler.h
 * Copyright (C) 2021 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_HANDLER_H
#define AVE_HANDLER_H

#include <memory>

#include "base/constructor_magic.h"
#include "looper.h"

namespace ave {
namespace media {

class Message;

class Handler : public std::enable_shared_from_this<Handler> {
 public:
   Handler()
       : id_(static_cast<int32_t>(0)),
         message_counter_(static_cast<uint32_t>(0)) {}
   virtual ~Handler() = default;

   Looper::handler_id id() const { return id_; }

   std::shared_ptr<Looper> looper() const { return looper_.lock(); }

   std::weak_ptr<Looper> getLooper() const { return looper_; }

 protected:
  virtual void onMessageReceived(const std::shared_ptr<Message>& message) = 0;

 private:
  friend class Message;
  friend class HandlerRoster;

  Looper::handler_id id_;
  std::weak_ptr<Looper> looper_;

  uint32_t message_counter_;

  inline void setId(Looper::handler_id id,
                    const std::weak_ptr<Looper>& looper) {
    id_ = id;
    looper_ = looper;
  }

  void deliverMessage(const std::shared_ptr<Message>& message);

  AVE_DISALLOW_COPY_AND_ASSIGN(Handler);
};

}  // namespace media
}  // namespace ave

#endif /* !AVE_HANDLER_H */
