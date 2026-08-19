# NetEq and video jitter-buffer integration

## Decision

`RtpSession` remains a packet-level data plane. NetEq and video jitter-buffer
implementations are separate receive pipelines registered through
`rtp_rtcp::RtpPacketSinkInterface`.

They must not be inserted after the current `AudioRtpReceiveStream` or
`VideoRtpReceiveStream`. Those classes already reorder/depacketize RTP and
convert it to `MediaFrame`, discarding packet-level information required by an
adaptive jitter buffer.

```text
PacketTransport -> RtpTransport -> RtpSession
                                      |
                         RtpPacketReceived routing
                                      |
              +-----------------------+-----------------------+
              |                       |                       |
    SimpleAudioReceive       NetEqAudioReceive       WebRtcVideoReceive
       (encoded frame)          (decoded PCM)       (decoded/encoded frame)
```

This preserves the existing simple receive streams for controlled LAN use and
allows WebRTC-grade receive pipelines to be selected per stream.

## Existing integration seam

`RtpSession::AddReceiveStream()` accepts a shared
`rtp_rtcp::RtpPacketSinkInterface`. A replacement pipeline can therefore
receive the routed `RtpPacketReceived` directly:

```cpp
class NetEqAudioReceiveStream final
    : public AsyncMediaFrameSource,
      public rtp_rtcp::RtpPacketSinkInterface {
 public:
  void OnRtpPacket(
      const rtp_rtcp::RtpPacketReceived& packet) override;

 private:
  // Decoder factory, payload mappings, NetEq instance, task runner, and
  // synchronization are owned by the adapter.
};
```

Registration is identical to the simple receive stream:

```cpp
auto receive_stream = std::make_shared<NetEqAudioReceiveStream>(config);
RtpReceiveStreamId stream_id = 0;
session->AddReceiveStream(
    {.media_type = RtpMediaType::kAudio,
     .payload_types = {111},
     .clock_rate_hz = 48000,
     .remote_ssrc = remote_ssrc},
    receive_stream, &stream_id);
```

The packet object provides the core data required by a jitter buffer:

- SSRC and payload type;
- RTP sequence number and timestamp;
- marker and payload;
- packet arrival time;
- negotiated RTP header extensions known to the session.

`RtpSession` continues to own transport demultiplexing, SSRC/MID routing,
receive statistics, and RTCP module dispatch. Codec and playout policy remain
inside the selected receive pipeline.

## NetEq audio pipeline

NetEq combines packet buffering, adaptive delay, codec decoding, packet-loss
concealment, and playout-rate adjustment. The current `AudioPacketBuffer` must
be bypassed; placing it before NetEq creates two jitter buffers and adds both
latency and misleading arrival behavior.

The recommended adapter has two execution paths:

```text
network receive sequence:
  OnRtpPacket -> translate header/arrival metadata -> NetEq InsertPacket

10 ms audio playout sequence:
  scheduled tick -> NetEq GetAudio -> PCM MediaFrame -> audio renderer
```

Network arrival must not drive playback directly. `InsertPacket()` and
`GetAudio()` must be serialized using one task runner or adapter-local locking,
according to the selected NetEq implementation's threading contract.

The adapter owns:

- negotiated payload-type-to-codec mappings;
- the NetEq decoder factory and NetEq instance;
- the 10 ms playout scheduler;
- conversion from NetEq audio output to PCM `MediaFrame`;
- NetEq lifetime, reset, and statistics.

For applications that need encoded packets for recording or forwarding, fan
out at the packet/encoded-frame boundary before NetEq. NetEq output is decoded
PCM and is not a replacement for the original encoded stream.

## Video jitter-buffer pipeline

A video adapter can use the same packet-sink seam:

```cpp
class WebRtcVideoReceiveStream final
    : public AsyncMediaFrameSource,
      public rtp_rtcp::RtpPacketSinkInterface {
 public:
  void OnRtpPacket(
      const rtp_rtcp::RtpPacketReceived& packet) override;
};
```

Unlike the current one-access-unit H.264 assembler, a WebRTC-grade video
pipeline normally contains:

1. RTX/RED/FEC recovery and media-packet normalization;
2. codec RTP depacketization;
3. a bounded multi-frame packet buffer;
4. frame-boundary and decode-dependency discovery;
5. NACK generation and key-frame requests;
6. decode scheduling, jitter estimation, and render timing;
7. encoded-frame delivery to the decoder or decoded-frame delivery to render.

The existing `VideoRtpReceiveStream` remains useful for low-loss LAN paths. It
must not precede another video jitter buffer because it has already removed RTP
sequence and fragmentation information and drops an incomplete access unit as
soon as a new RTP timestamp arrives.

## Interfaces still required for full WebRTC interoperability

### Negotiated RTP header extensions

The current session configuration exposes only a MID extension ID. Full
WebRTC receive pipelines need the complete negotiated extension map, including
applicable extensions such as:

- MID and repaired RTP stream ID;
- audio level;
- absolute capture/send time;
- transport sequence number;
- video orientation and playout delay;
- generic frame or dependency descriptors.

Evolve `RtpSession::Config` toward receiving a negotiated
`RtpHeaderExtensionMap` rather than adding one configuration field for every
extension. The session should use that map when parsing every
`RtpPacketReceived`.

### Receive feedback

Jitter buffers detect loss and decoder recovery needs, but transport feedback
must be sent through the appropriate RTP/RTCP module. Introduce a narrow
interface owned by the receive pipeline, for example:

```cpp
class RtpReceiveFeedback {
 public:
  virtual ~RtpReceiveFeedback() = default;
  virtual void SendNack(
      uint32_t media_ssrc,
      std::span<const uint16_t> sequence_numbers) = 0;
  virtual void RequestKeyFrame(uint32_t media_ssrc) = 0;
  virtual void SendTransportFeedback(/* negotiated feedback data */) = 0;
};
```

An implementation can translate these calls into NACK, PLI/FIR, and transport
feedback through `RtpRtcpInterface`. This keeps feedback policy out of
`RtpSession` while retaining one RTCP transport and module registry.

### Recovered-packet routing

RTX and FEC output must re-enter the receive pipeline at a clearly defined
stage. Recovered packets should not be treated as new network datagrams for
endpoint learning or SRTP authentication. They should be normalized to their
media SSRC/sequence space and inserted between session routing and the media
jitter buffer.

## Proposed implementation order

1. Extend `RtpSession::Config` with a complete negotiated extension map.
2. Add an `RtpReceiveFeedback` adapter backed by `RtpRtcpInterface`.
3. Implement `NetEqAudioReceiveStream` and its independent 10 ms playout task.
4. Add NetEq delay, concealment, packet-loss, and underrun statistics.
5. Define the recovered-packet stage and implement RTX/NACK for video.
6. Implement a multi-frame video receive pipeline and decoder scheduling.
7. Add interoperability tests using captured WebRTC RTP/RTCP and live peers.

This work does not require moving NetEq or the video jitter buffer into
`RtpSession`. Keeping them as selectable packet consumers preserves the simple
LAN path and prevents the session/transport layer from depending on codec and
playout policy.
