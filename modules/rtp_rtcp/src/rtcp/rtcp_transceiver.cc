/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/modules/rtp_rtcp/src/rtcp/rtcp_transceiver.h"
#include <span>

#include <memory>
#include <utility>
#include <vector>

#include "base/checks.h"
#include "base/task_util/task.h"
#include "base/task_util/to_task.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

RtcpTransceiver::RtcpTransceiver(const RtcpTransceiverConfig& config)
    : clock_(config.clock),
      task_runner_(config.task_runner),
      rtcp_transceiver_(std::make_unique<RtcpTransceiverImpl>(config)) {
  AVE_DCHECK(rtcp_transceiver_);
}

RtcpTransceiver::~RtcpTransceiver() {
  if (rtcp_transceiver_) {
    // Stop on destruction if not already stopped.
    rtcp_transceiver_->StopPeriodicTask();
    rtcp_transceiver_.reset();
  }
}

void RtcpTransceiver::Stop(std::function<void()> on_destroyed) {
  AVE_DCHECK(rtcp_transceiver_);
  rtcp_transceiver_->StopPeriodicTask();

  if (task_runner_) {
    task_runner_->PostTask(
        base::toTask([transceiver = std::move(rtcp_transceiver_),
                      callback = std::move(on_destroyed)]() {
          // Release the transceiver on the task queue thread
          // Then call the callback
          if (callback) {
            callback();
          }
        }));
  } else {
    rtcp_transceiver_.reset();
    if (on_destroyed) {
      on_destroyed();
    }
  }
}

void RtcpTransceiver::AddMediaReceiverRtcpObserver(
    uint32_t remote_ssrc,
    MediaReceiverRtcpObserver* observer) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(base::toTask([this, remote_ssrc, observer]() {
      rtcp_transceiver_->AddMediaReceiverRtcpObserver(remote_ssrc, observer);
    }));
  } else {
    rtcp_transceiver_->AddMediaReceiverRtcpObserver(remote_ssrc, observer);
  }
}

void RtcpTransceiver::RemoveMediaReceiverRtcpObserver(
    uint32_t remote_ssrc,
    MediaReceiverRtcpObserver* observer,
    std::function<void()> on_removed) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(base::toTask([this, remote_ssrc, observer,
                                         on_removed =
                                             std::move(on_removed)]() mutable {
      rtcp_transceiver_->RemoveMediaReceiverRtcpObserver(remote_ssrc, observer);
      if (on_removed) {
        on_removed();
      }
    }));
  } else {
    rtcp_transceiver_->RemoveMediaReceiverRtcpObserver(remote_ssrc, observer);
    if (on_removed) {
      on_removed();
    }
  }
}

void RtcpTransceiver::SetReadyToSend(bool ready) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(base::toTask(
        [this, ready]() { rtcp_transceiver_->SetReadyToSend(ready); }));
  } else {
    rtcp_transceiver_->SetReadyToSend(ready);
  }
}

void RtcpTransceiver::ReceivePacket(base::CopyOnWriteBuffer packet) {
  AVE_DCHECK(rtcp_transceiver_);
  auto now = clock_->CurrentTime();
  if (task_runner_) {
    task_runner_->PostTask(
        base::toTask([this, packet = std::move(packet), now]() {
          rtcp_transceiver_->ReceivePacket(
              std::span<const uint8_t>(packet.data(), packet.size()), now);
        }));
  } else {
    rtcp_transceiver_->ReceivePacket(
        std::span<const uint8_t>(packet.data(), packet.size()), now);
  }
}

void RtcpTransceiver::SendCompoundPacket() {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(
        base::toTask([this]() { rtcp_transceiver_->SendCompoundPacket(); }));
  } else {
    rtcp_transceiver_->SendCompoundPacket();
  }
}

void RtcpTransceiver::SetRemb(int64_t bitrate_bps,
                              std::vector<uint32_t> ssrcs) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(
        base::toTask([this, bitrate_bps, ssrcs = std::move(ssrcs)]() mutable {
          rtcp_transceiver_->SetRemb(bitrate_bps, std::move(ssrcs));
        }));
  } else {
    rtcp_transceiver_->SetRemb(bitrate_bps, std::move(ssrcs));
  }
}

void RtcpTransceiver::UnsetRemb() {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(
        base::toTask([this]() { rtcp_transceiver_->UnsetRemb(); }));
  } else {
    rtcp_transceiver_->UnsetRemb();
  }
}

void RtcpTransceiver::SendCombinedRtcpPacket(
    std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(
        base::toTask([this, rtcp_packets = std::move(rtcp_packets)]() mutable {
          rtcp_transceiver_->SendCombinedRtcpPacket(std::move(rtcp_packets));
        }));
  } else {
    rtcp_transceiver_->SendCombinedRtcpPacket(std::move(rtcp_packets));
  }
}

void RtcpTransceiver::SendNack(uint32_t ssrc,
                               std::vector<uint16_t> sequence_numbers) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(base::toTask(
        [this, ssrc, sequence_numbers = std::move(sequence_numbers)]() mutable {
          rtcp_transceiver_->SendNack(ssrc, std::move(sequence_numbers));
        }));
  } else {
    rtcp_transceiver_->SendNack(ssrc, std::move(sequence_numbers));
  }
}

void RtcpTransceiver::SendPictureLossIndication(uint32_t ssrc) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(base::toTask([this, ssrc]() {
      rtcp_transceiver_->SendPictureLossIndication(ssrc);
    }));
  } else {
    rtcp_transceiver_->SendPictureLossIndication(ssrc);
  }
}

void RtcpTransceiver::SendFullIntraRequest(std::vector<uint32_t> ssrcs) {
  SendFullIntraRequest(std::move(ssrcs), true);
}

void RtcpTransceiver::SendFullIntraRequest(std::vector<uint32_t> ssrcs,
                                           bool new_request) {
  AVE_DCHECK(rtcp_transceiver_);
  if (task_runner_) {
    task_runner_->PostTask(base::toTask([this, ssrcs = std::move(ssrcs),
                                         new_request]() mutable {
      rtcp_transceiver_->SendFullIntraRequest(
          std::span<const uint32_t>(ssrcs.data(), ssrcs.size()), new_request);
    }));
  } else {
    rtcp_transceiver_->SendFullIntraRequest(
        std::span<const uint32_t>(ssrcs.data(), ssrcs.size()), new_request);
  }
}

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave
