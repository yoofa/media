/*
 * message_object.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MESSAGE_OBJECT_H
#define MESSAGE_OBJECT_H

namespace ave {
namespace media {

class MessageObject {
 public:
  MessageObject() = default;
  virtual ~MessageObject() = default;
};

}  // namespace media
}  // namespace ave

#endif /* !MESSAGE_OBJECT_H */
