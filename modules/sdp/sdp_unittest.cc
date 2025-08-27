#include "media/modules/sdp/sdp.h"

#include "test/gtest.h"

using namespace ave::media::sdp;

static const char* kSample =
    "v=0\r\n"
    "o=- 2890844526 2890842807 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 0 1\r\n"
    "a=msid-semantic: WMS stream1\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=mid:0\r\n"
    "a=sendrecv\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
    "a=ice-ufrag:UfRg\r\n"
    "a=ice-pwd:pWd1234567890\r\n"
    "a=fingerprint:sha-256 "
    "12:34:56:78:9A:BC:DE:F0:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
    "01:23:45:67:89:AB:CD:EF\r\n"
    "a=setup:actpass\r\n"
    "a=candidate:842163049 1 udp 1677729535 192.168.1.2 56143 typ srflx raddr "
    "0.0.0.0 rport 9 generation 0\r\n"
    "a=msid:stream1 track1\r\n"
    "a=ssrc-group:FID 11111111 22222222\r\n"
    "a=ssrc:11111111 cname:audio@example\r\n"
    "a=ssrc:11111111 msid:stream1 track1\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "a=mid:1\r\n"
    "a=sendonly\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:96 VP8/90000\r\n";

TEST(SdpTest, ParseBasicAndSerializeRoundtrip) {
  SessionDescription sdp;
  std::string err;
  ASSERT_TRUE(Parse(kSample, &sdp, &err)) << err;
  ASSERT_EQ(2u, sdp.media.size());
  const auto& audio = sdp.media[0];
  EXPECT_EQ(MediaKind::kAudio, audio.kind);
  EXPECT_EQ("0", audio.mid);
  EXPECT_EQ(Direction::kSendRecv, audio.direction);
  EXPECT_TRUE(audio.rtcp_mux);
  ASSERT_EQ(1u, audio.rtp_maps.size());
  EXPECT_EQ(111, audio.rtp_maps[0].payload_type);
  EXPECT_EQ("opus", audio.rtp_maps[0].encoding);
  EXPECT_EQ(48000, audio.rtp_maps[0].clock_rate_hz);
  EXPECT_EQ(2, audio.rtp_maps[0].channels);
  EXPECT_EQ("UfRg", audio.ice_ufrag);
  EXPECT_EQ("pWd1234567890", audio.ice_pwd);
  EXPECT_EQ("sha-256", audio.dtls_fingerprint_algo);
  EXPECT_EQ(DtlsSetup::kActpass, audio.dtls_setup);
  ASSERT_FALSE(audio.ice_candidates.empty());
  EXPECT_EQ(56143, audio.ice_candidates[0].port);
  ASSERT_FALSE(audio.msids.empty());
  EXPECT_EQ("stream1", audio.msids[0].stream_id);
  ASSERT_FALSE(audio.ssrc_groups.empty());
  EXPECT_EQ("FID", audio.ssrc_groups[0].semantics);
  ASSERT_FALSE(audio.ssrcs.empty());
  EXPECT_EQ(11111111u, audio.ssrcs[0].ssrc);

  auto out = Serialize(sdp);
  // Should at least contain key lines back
  EXPECT_NE(out.find("a=ice-ufrag:UfRg"), std::string::npos);
  EXPECT_NE(out.find("a=ssrc-group:FID"), std::string::npos);
}
