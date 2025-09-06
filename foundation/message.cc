/*
 * Message.cc
 * Copyright (C) 2021 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "message.h"

#include <iostream>
#include <memory>

#include "base/errors.h"
#include "handler.h"
#include "looper.h"

namespace ave {
namespace media {

status_t ReplyToken::setReply(const std::shared_ptr<Message>& reply) {
  if (replied_) {
    return -1;
  }
  std::cout << "setReply" << std::endl;
  // AVE_CHECK(reply_ == nullptr);
  reply_ = reply;
  replied_ = true;
  return 0;
}

Message::Message()
    : what_(static_cast<uint32_t>(0)), handler_id_(static_cast<int32_t>(0)) {}

Message::Message(uint32_t what, const std::shared_ptr<Handler>& handler)
    : what_(what), handler_id_(static_cast<int32_t>(0)) {
  setHandler(handler);
}

Message::~Message() {
  clear();
}

void Message::setWhat(uint32_t what) {
  what_ = what;
}

uint32_t Message::what() const {
  return what_;
}

void Message::setHandler(const std::shared_ptr<Handler>& handler) {
  if (handler == nullptr) {
    handler_id_ = static_cast<int32_t>(0);
    handler_.reset();
    looper_.reset();
  } else {
    handler_id_ = handler->id();
    handler_ = handler;
    looper_ = handler->getLooper();
  }
}

status_t Message::post(int64_t delay_us) {
  auto looper = looper_.lock();
  if (looper != nullptr) {
    looper->post(shared_from_this(), delay_us);
  }
  return 0;
}

status_t Message::postAndWaitResponse(std::shared_ptr<Message>& response) {
  std::shared_ptr<Looper> looper = looper_.lock();
  if (looper == nullptr) {
    return -1;
  }

  std::shared_ptr<ReplyToken> replyToken = looper->createReplyToken();
  if (replyToken == nullptr) {
    return -1;
  }

  setReplyToken("replyID", replyToken);
  looper->post(shared_from_this(), 0);
  return looper->awaitResponse(replyToken, response);
}

bool Message::senderAwaitsResponse(std::shared_ptr<ReplyToken>& replyId) {
  bool found = findReplyToken("replyID", replyId);
  if (!found) {
    return false;
  }
  return replyId != nullptr;
}

status_t Message::postReply(const std::shared_ptr<ReplyToken>& replyId) {
  if (replyId == nullptr) {
    return -1;
  }

  std::shared_ptr<Looper> looper = replyId->getLooper();
  if (looper == nullptr) {
    return -1;
  }

  return looper->postReply(replyId, shared_from_this());
}

void Message::clear() {
  objects_.clear();
}

bool Message::contains(const char* name) const {
  auto search = objects_.find(name);
  return search != objects_.end();
}

void Message::setObject(const char* name, const std::any& obj) {
  objects_[name] = obj;
}

#define BASIC_TYPE(NAME, ...)                                            \
  void Message::set##NAME(const char* name, __VA_ARGS__ value) {         \
    setObject(name, value);                                              \
  }                                                                      \
  bool Message::find##NAME(const char* name, __VA_ARGS__* value) const { \
    return findObject(name, *value);                                     \
  }

BASIC_TYPE(Int32, int32_t)
BASIC_TYPE(Int64, int64_t)
BASIC_TYPE(Size, size_t)
BASIC_TYPE(Float, float)
BASIC_TYPE(Double, double)
BASIC_TYPE(Pointer, void*)

#undef BASIC_TYPE

void Message::setRect(const char* name,
                      int32_t left,
                      int32_t top,
                      int32_t right,
                      int32_t bottom) {
  Rect rect{left, top, right, bottom};
  setObject(name, rect);
}

bool Message::findRect(const char* name,
                       int32_t* left,
                       int32_t* top,
                       int32_t* right,
                       int32_t* bottom) const {
  if (!left || !top || !right || !bottom) {
    return false;
  }
  Rect rect{};
  if (!findObject(name, rect)) {
    return false;
  }

  *left = rect.left_;
  *top = rect.top_;
  *right = rect.right_;
  *bottom = rect.bottom_;
  return true;
}

void Message::setString(const char* name, const std::string& s) {
  return setObject(name, s);
}

void Message::setString(const char* name, const char* s, ssize_t len) {
  return setObject(name, std::string(s, (len < 0) ? strlen(s) : len));
}

bool Message::findString(const char* name, std::string& value) const {
  return findObject(name, value);
}

void Message::setMessage(const char* name, std::shared_ptr<Message> msg) {
  return setObject(name, msg);
}

bool Message::findMessage(const char* name,
                          std::shared_ptr<Message>& msg) const {
  return findObject(name, msg);
}

void Message::setReplyToken(const char* name,
                            std::shared_ptr<ReplyToken> token) {
  setObject(name, token);
}

bool Message::findReplyToken(const char* name,
                             std::shared_ptr<ReplyToken>& token) const {
  return findObject(name, token);
}

void Message::setBuffer(const char* name,
                        std::shared_ptr<ave::media::Buffer> buffer) {
  setObject(name, buffer);
}

bool Message::findBuffer(const char* name,
                         std::shared_ptr<ave::media::Buffer>& buffer) const {
  return findObject(name, buffer);
}

std::shared_ptr<Message> Message::dup() const {
  std::shared_ptr<Message> message =
      std::make_shared<Message>(what_, handler_.lock());

  return message;
}

void Message::deliver() {
  auto handler = handler_.lock();
  if (handler != nullptr) {
    handler->deliverMessage(shared_from_this());
  }
}

}  // namespace media
}  // namespace ave
