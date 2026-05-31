/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/src/util/dtmf_queue.h"

#include <cstddef>

#include "base/checks.h"

namespace {
constexpr size_t kDtmfOutbandMax = 20;
}

namespace ave {
namespace media {
namespace rtp_rtcp {

using ::ave::base::lock_guard;

DtmfQueue::DtmfQueue() = default;

DtmfQueue::~DtmfQueue() = default;

bool DtmfQueue::AddDtmf(const Event& event) {
  lock_guard lock(dtmf_mutex_);
  if (queue_.size() >= kDtmfOutbandMax) {
    return false;
  }
  queue_.push_back(event);
  return true;
}

bool DtmfQueue::NextDtmf(Event* event) {
  AVE_DCHECK(event);
  lock_guard lock(dtmf_mutex_);
  if (queue_.empty()) {
    return false;
  }

  *event = queue_.front();
  queue_.pop_front();
  return true;
}

bool DtmfQueue::PendingDtmf() const {
  lock_guard lock(dtmf_mutex_);
  return !queue_.empty();
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
