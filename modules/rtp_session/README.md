# RTP session

`rtp_session` is the reusable encoded-media transport layer used by the AVE
applications. It packetizes encoded audio/H.264 frames, carries multiplexed
RTP and RTCP over a packet transport, optionally applies SRTP, and routes
received RTP to one or more encoded-frame streams.

It deliberately does **not** own capture, codecs, rendering, signaling, SDP,
or protocol-specific session setup. Those remain application or protocol
adapter responsibilities. See [DESIGN.md](DESIGN.md) for the boundaries and
extension model. See
[NETEQ_JITTER_BUFFER_INTEGRATION.md](NETEQ_JITTER_BUFFER_INTEGRATION.md) for
the planned NetEq and WebRTC video receive-pipeline integration.

## Targets

- `packet_transport`: datagram abstraction and the IPv4 UDP implementation.
- `srtp_session`: libSRTP protection using the WebRTC-compatible
  `AES_CM_128_HMAC_SHA1_80` profile.
- `rtp_transport`: RTP/RTCP mux, remote endpoint handling, and optional SRTP.
- `rtp_session`: multi-stream routing, RTCP modules, encoded audio streams,
  and RFC 6184 H.264 streams.

Build and run the module tests from the repository root:

```sh
gn gen out/Default
ninja -C out/Default rtp_session_unittests
out/Default/rtp_session_unittests
```

## Receiving encoded audio

The following shows the ownership and startup order. Error handling is omitted
for brevity.

```cpp
using namespace ave::media::rtp_session;

auto local = PacketEndpoint::CreateIpv4("0.0.0.0", 5000).value();
auto udp = std::make_unique<UdpPacketTransport>(UdpPacketTransport::Config{
    .local_endpoint = local,
    .clock = ave::base::Clock::GetRealTimeClock(),
});
auto transport = std::make_unique<RtpTransport>(RtpTransport::Config{
    .packet_transport = std::move(udp),
});
auto session = std::make_unique<RtpSession>(RtpSession::Config{
    .transport = std::move(transport),
    .clock = ave::base::Clock::GetRealTimeClock(),
});

constexpr uint32_t kRemoteSsrc = 0x12345678;
auto audio = std::make_shared<AudioRtpReceiveStream>(
    AudioRtpReceiveStream::Config{
        .remote_ssrc = kRemoteSsrc,
        .rtp_clock_rate_hz = 48000,
        .payload_type_codecs = {{96, CodecId::AVE_CODEC_ID_OPUS}},
        .task_runner_factory = task_runner_factory,
    });
audio->AddOrUpdateSink(encoded_audio_decoder, {});

RtpReceiveStreamId stream_id = 0;
session->AddReceiveStream(
    {.media_type = RtpMediaType::kAudio,
     .payload_types = {96},
     .clock_rate_hz = 48000,
     .remote_ssrc = kRemoteSsrc},
    audio, &stream_id);
audio->Start();
session->Start();
```

Shutdown in the reverse order: stop the session, stop the receive stream,
remove its media sink, then destroy the objects. A sink and an observer passed
as a raw pointer must outlive the object that uses it.

`VideoRtpReceiveStream` is wired the same way. It accepts payload type 102 by
default and emits complete Annex-B H.264 access units. Its downstream sink can
be a platform decoder, recorder, muxer, or another fan-out node.

## Sending encoded frames

Sending requires one `RtpRtcpInterface` module per local SSRC. Configure it
with `session->outgoing_transport()`, add it to the session, then create an
`AudioRtpSendStream` or `VideoRtpSendStream` using the module pointer. Set the
module's sending and RTCP status before starting the session.

```cpp
rtp_rtcp::RtpRtcpInterface::Configuration config;
config.audio = false;
config.outgoing_transport = session->outgoing_transport();
config.receive_statistics = session->receive_statistics();
config.local_media_ssrc = local_ssrc;
config.task_runner = worker_queue;

auto module = std::make_unique<rtp_rtcp::ModuleRtpRtcpImpl2>(
    clock, worker_queue, config);
auto* module_ptr = module.get();
module_ptr->SetRTCPStatus(rtp_rtcp::RtcpMode::kCompound);
module_ptr->SetSendingStatus(true);
module_ptr->SetSendingMediaStatus(true);
session->AddRtpRtcpModule(std::move(module));

VideoRtpSendStream sender({
    .payload_type = 102,
    .rtp_clock_rate_hz = 90000,
    .rtp_rtcp = module_ptr,
    .clock = clock,
});
encoder->AddOrUpdateSink(&sender, {});
```

Video input must be one complete Annex-B H.264 access unit per `MediaFrame`,
with finite PTS, valid dimensions, and key-frame metadata. Audio input must be
one complete encoded audio packet per frame. The send streams packetize only;
they do not encode or resample.

The complete construction patterns are also covered by
`rtp_session_unittest.cc` and `video_rtp_send_stream_unittest.cc`.

## Security

Set `RtpTransport::Config::security.mode` to
`RtpSecurityMode::kSrtpPreSharedKey` and provide independent inbound and
outbound 30-byte master key/salt values. Key exchange is intentionally outside
this module. A WebRTC-compatible endpoint normally obtains those keys through
DTLS-SRTP; never ship fixed or all-zero keys.

## Integration checklist

1. Negotiate payload types, clock rates, SSRCs, MID extension ID, codecs, and
   SRTP keys in the application signaling layer.
2. Create UDP/another `PacketTransport`, then `RtpTransport`, then `RtpSession`.
3. Add receive streams and RTP/RTCP modules while the session is stopped.
4. Start downstream consumers before the session; stop the session first.
5. Poll `RtpSession::GetStats()` and stream-specific stats for diagnostics.
6. For bursty video, tune `UdpPacketTransport` socket buffers and add pacing in
   the sender pipeline; large buffers are not a substitute for pacing.
