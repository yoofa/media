# RTP session design

## Goals

The module provides a compact, reusable RTP/RTCP data plane for low-latency
audio/video applications. Its encoded-frame boundary allows one encoder output
to fan out simultaneously to RTP, a muxer, an RTSP server, or an application
specific transport without coupling the encoder to RTP.

The wire format follows the RTP/RTCP and H.264 packetization behavior used by
WebRTC. Interoperability still depends on the signaling layer agreeing on SDP
semantics, payload types, header extensions, codec parameters, RTCP feedback,
and SRTP keying.

## Layers and ownership

```text
encoded MediaFrame sources/sinks
        |                         ^
Audio/VideoRtpSendStream   Audio/VideoRtpReceiveStream
        |                         ^
        +---- RtpRtcpInterface ---+
                       |
                  RtpSession
        routing, RTCP fan-out, statistics
                       |
                  RtpTransport
       RTP/RTCP mux, endpoint policy, SRTP
                       |
                 PacketTransport
                  UDP or adapter
```

- `RtpSession` owns its `RtpTransport` and every added
  `RtpRtcpInterface` module.
- Receive sinks are held by `shared_ptr`; stream-specific media sinks and
  observers are non-owning and must outlive registration.
- Receive-stream routes are synchronized and may be added or removed at run
  time. RTP/RTCP modules may be added or removed only while the session is
  stopped.
- `PacketTransport::Stop()` must prevent further callbacks before it returns.

This lifetime contract lets RTCP be dispatched without holding the session
mutex and avoids lock inversion when an RTP/RTCP module calls back into the
transport.

## Receive routing

An incoming datagram is authenticated/decrypted first, then classified as RTP
or RTCP. RTP routing uses this precedence:

1. negotiated MID header extension;
2. previously learned SSRC;
3. a single stream whose payload type is discoverable;
4. the optional application receive-stream factory.

The selected SSRC is learned for subsequent packets. Ambiguous payload types
are never guessed. MID is authoritative: a packet whose MID identifies a
stream but whose payload type is not configured for it is discarded.

RTCP compound packets are validated and delivered to every registered local
RTP/RTCP module. Applications can observe accepted and discarded packets and
read aggregate/per-stream counters.

## Media timelines

Send streams translate the first finite media PTS to the RTP module's random
start timestamp and preserve elapsed time in the negotiated RTP clock.

Receive streams unwrap the 32-bit RTP timestamp and expose a timeline starting
at zero. Normal wrap-around stays continuous. A backward jump or a gap larger
than five seconds is treated as a sender restart; a new epoch begins just after
the last emitted PTS. This keeps decoder and renderer timestamps monotonic when
a sender restarts without changing SSRC.

## Audio and video behavior

Audio receive uses a small sequence-number reorder buffer. It emits encoded
packets and derives frame duration from adjacent RTP timestamps when safe.

H.264 send accepts Annex-B access units and uses RFC 6184 non-interleaved
packetization (single NALU, STAP-A, and FU-A as selected by the RTP sender).
H.264 receive reorders packets within one RTP timestamp, validates FU-A
fragment state and STAP-A lengths, and emits Annex-B access units only after a
contiguous marker-terminated frame is available.

The current video assembler intentionally favors latency and simplicity: it
buffers one access unit and drops it when packets from a new RTP timestamp
arrive before completion. Production lossy-network deployments should add a
bounded multi-frame jitter buffer, NACK/RTX, key-frame requests, and loss-aware
decoder recovery.

## Transport and security

`PacketTransport` is the protocol-independent datagram seam. The supplied UDP
implementation is IPv4, has an interruptible receive thread, detects truncated
datagrams, and exposes configurable kernel send/receive buffers.

`RtpTransport` multiplexes RTP and RTCP on one packet transport. It supports a
configured remote RTP endpoint and learns RTCP return endpoints from accepted
RTP sources. SRTP/SRTCP protection uses separate inbound/outbound libSRTP
contexts with replay protection.

Only pre-shared SRTP keys are implemented here. DTLS, ICE, consent checks, key
rotation, and certificate verification belong in signaling/transport adapters.

## Protocol adapters

- **AVE private transport:** use the module directly and define signaling for
  endpoint, payload, SSRC/MID, codec, and key negotiation.
- **WebRTC:** retain this RTP data plane, but supply SDP offer/answer, ICE,
  DTLS-SRTP, negotiated extensions, RTCP feedback, pacing, retransmission, and
  congestion control in surrounding components.
- **Miracast:** reuse codecs, frames, clocks, and selected RTP primitives, but
  implement the RTSP/WFD control plane and its negotiated RTP port topology in
  a Miracast adapter.
- **AirPlay:** reuse codec/render and encoded-frame plumbing. AirPlay session
  setup, encryption, timing, retransmission, and packet formats require a
  separate adapter and must not be forced through `RtpSession` when they differ
  from standard RTP.

Keep protocol policy above `PacketTransport`/`RtpSession`; keep codecs and
renderers below the encoded `MediaFrame` boundary. This preserves the ability
to share one decoder or encoder while independently replacing network
protocols.

## Known gaps before production use

- no SDP/ICE/DTLS or automatic key exchange;
- no sender pacer, bandwidth estimator, congestion controller, or transport-cc;
- no complete NACK/RTX/FEC/PLI recovery pipeline;
- no bounded multi-frame video jitter buffer;
- no IPv6 UDP implementation;
- no audio concealment, adaptive jitter target, or clock-drift correction;
- no endpoint admission/authentication outside SRTP packet validation.
