/*
 * Handler.cc
 * Copyright (C) 2021 youfa.song <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "handler.h"

namespace ave {
namespace media {

void Handler::deliverMessage(const std::shared_ptr<Message>& message) {
  onMessageReceived(message);
  message_counter_++;
}

}  // namespace media
}  // namespace ave
