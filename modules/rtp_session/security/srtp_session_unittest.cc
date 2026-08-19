#include "media/modules/rtp_session/security/srtp_session.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "test/gtest.h"

namespace ave::media::rtp_session {
namespace {

SrtpKey MakeKey(uint8_t seed) {
  SrtpKey key;
  for (size_t index = 0; index < key.bytes.size(); ++index) {
    key.bytes[index] = static_cast<uint8_t>(seed + index);
  }
  return key;
}

SrtpSessionConfig MakeConfig(const SrtpKey& outbound_key,
                             const SrtpKey& inbound_key) {
  return {
      .mode = RtpSecurityMode::kSrtpPreSharedKey,
      .outbound_key = outbound_key,
      .inbound_key = inbound_key,
      .replay_window_size = 128,
  };
}

std::vector<uint8_t> MakeRtpPacket(uint16_t sequence_number,
                                   uint32_t ssrc,
                                   uint8_t payload_seed = 0x40) {
  std::vector<uint8_t> packet{
      0x80,
      0x60,
      static_cast<uint8_t>(sequence_number >> 8),
      static_cast<uint8_t>(sequence_number),
      0x01,
      0x02,
      0x03,
      0x04,
      static_cast<uint8_t>(ssrc >> 24),
      static_cast<uint8_t>(ssrc >> 16),
      static_cast<uint8_t>(ssrc >> 8),
      static_cast<uint8_t>(ssrc),
  };
  for (uint8_t offset = 0; offset < 24; ++offset) {
    packet.push_back(static_cast<uint8_t>(payload_seed + offset));
  }
  return packet;
}

std::vector<uint8_t> MakeRtcpPacket(uint32_t ssrc) {
  return {
      0x80,
      0xc8,
      0x00,
      0x06,
      static_cast<uint8_t>(ssrc >> 24),
      static_cast<uint8_t>(ssrc >> 16),
      static_cast<uint8_t>(ssrc >> 8),
      static_cast<uint8_t>(ssrc),
      0x00,
      0x00,
      0x00,
      0x01,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      0x00,
      0x03,
      0x00,
      0x00,
      0x00,
      0x04,
      0x00,
      0x00,
      0x00,
      0x05,
  };
}

TEST(SrtpSessionTest, NoneModePreservesPackets) {
  SrtpSession session(SrtpSessionConfig{});
  ASSERT_EQ(session.init_status(), OK);
  EXPECT_EQ(session.mode(), RtpSecurityMode::kNone);

  const std::vector<uint8_t> packet = MakeRtpPacket(1, 0x11223344);
  std::vector<uint8_t> protected_packet;
  std::vector<uint8_t> unprotected_packet;
  ASSERT_EQ(session.Protect(SrtpPacketType::kRtp, packet, &protected_packet),
            OK);
  EXPECT_EQ(protected_packet, packet);
  ASSERT_EQ(session.Unprotect(SrtpPacketType::kRtp, protected_packet,
                              &unprotected_packet),
            OK);
  EXPECT_EQ(unprotected_packet, packet);
}

TEST(SrtpSessionTest, ProtectsRtpAndRtcpRoundTrip) {
  const SrtpKey first_key = MakeKey(0x10);
  const SrtpKey second_key = MakeKey(0x80);
  SrtpSession sender(MakeConfig(first_key, second_key));
  SrtpSession receiver(MakeConfig(second_key, first_key));
  ASSERT_EQ(sender.init_status(), OK);
  ASSERT_EQ(receiver.init_status(), OK);

  const std::vector<uint8_t> rtp_packet = MakeRtpPacket(100, 0x11223344);
  std::vector<uint8_t> protected_rtp;
  std::vector<uint8_t> received_rtp;
  ASSERT_EQ(sender.Protect(SrtpPacketType::kRtp, rtp_packet, &protected_rtp),
            OK);
  ASSERT_GT(protected_rtp.size(), rtp_packet.size());
  EXPECT_FALSE(std::equal(rtp_packet.begin() + 12, rtp_packet.end(),
                          protected_rtp.begin() + 12));
  EXPECT_FALSE(IsRtcpMuxPacket(protected_rtp));
  ASSERT_EQ(
      receiver.Unprotect(SrtpPacketType::kRtp, protected_rtp, &received_rtp),
      OK);
  EXPECT_EQ(received_rtp, rtp_packet);

  const std::vector<uint8_t> rtcp_packet = MakeRtcpPacket(0x11223344);
  std::vector<uint8_t> protected_rtcp;
  std::vector<uint8_t> received_rtcp;
  ASSERT_EQ(sender.Protect(SrtpPacketType::kRtcp, rtcp_packet, &protected_rtcp),
            OK);
  ASSERT_GT(protected_rtcp.size(), rtcp_packet.size());
  EXPECT_TRUE(IsRtcpMuxPacket(protected_rtcp));
  ASSERT_EQ(
      receiver.Unprotect(SrtpPacketType::kRtcp, protected_rtcp, &received_rtcp),
      OK);
  EXPECT_EQ(received_rtcp, rtcp_packet);
}

TEST(SrtpSessionTest, SupportsMultipleSsrcAndSequenceRollover) {
  const SrtpKey key = MakeKey(0x20);
  SrtpSession sender(MakeConfig(key, key));
  SrtpSession receiver(MakeConfig(key, key));
  ASSERT_EQ(sender.init_status(), OK);
  ASSERT_EQ(receiver.init_status(), OK);

  constexpr std::array<uint16_t, 4> kRolloverSequenceNumbers{65534, 65535, 0,
                                                             1};
  for (uint16_t sequence_number : kRolloverSequenceNumbers) {
    const std::vector<uint8_t> packet =
        MakeRtpPacket(sequence_number, 0x01020304);
    std::vector<uint8_t> protected_packet;
    std::vector<uint8_t> received_packet;
    ASSERT_EQ(sender.Protect(SrtpPacketType::kRtp, packet, &protected_packet),
              OK);
    ASSERT_EQ(receiver.Unprotect(SrtpPacketType::kRtp, protected_packet,
                                 &received_packet),
              OK);
    EXPECT_EQ(received_packet, packet);
  }

  const std::vector<uint8_t> second_ssrc_packet =
      MakeRtpPacket(7, 0xa1a2a3a4, 0x90);
  std::vector<uint8_t> protected_packet;
  std::vector<uint8_t> received_packet;
  ASSERT_EQ(sender.Protect(SrtpPacketType::kRtp, second_ssrc_packet,
                           &protected_packet),
            OK);
  ASSERT_EQ(receiver.Unprotect(SrtpPacketType::kRtp, protected_packet,
                               &received_packet),
            OK);
  EXPECT_EQ(received_packet, second_ssrc_packet);
}

TEST(SrtpSessionTest, SustainsTrafficAcrossRolloverWithoutAuthFailures) {
  const SrtpKey key = MakeKey(0x28);
  SrtpSession sender(MakeConfig(key, key));
  SrtpSession receiver(MakeConfig(key, key));
  ASSERT_EQ(sender.init_status(), OK);
  ASSERT_EQ(receiver.init_status(), OK);

  std::vector<uint8_t> packet = MakeRtpPacket(0, 0x0a0b0c0d);
  std::vector<uint8_t> protected_packet;
  std::vector<uint8_t> received_packet;
  constexpr uint32_t kPacketCount = 70000;
  for (uint32_t packet_index = 0; packet_index < kPacketCount; ++packet_index) {
    const uint16_t sequence_number = static_cast<uint16_t>(packet_index);
    packet[2] = static_cast<uint8_t>(sequence_number >> 8);
    packet[3] = static_cast<uint8_t>(sequence_number);
    ASSERT_EQ(sender.Protect(SrtpPacketType::kRtp, packet, &protected_packet),
              OK)
        << "packet_index=" << packet_index;
    ASSERT_EQ(receiver.Unprotect(SrtpPacketType::kRtp, protected_packet,
                                 &received_packet),
              OK)
        << "packet_index=" << packet_index;
    ASSERT_EQ(received_packet, packet) << "packet_index=" << packet_index;
  }
}

TEST(SrtpSessionTest, RejectsTamperingWrongKeyAndReplay) {
  const SrtpKey key = MakeKey(0x30);
  SrtpSession sender(MakeConfig(key, key));
  SrtpSession receiver(MakeConfig(key, key));
  SrtpSession wrong_receiver(MakeConfig(MakeKey(0x70), MakeKey(0x71)));
  ASSERT_EQ(sender.init_status(), OK);
  ASSERT_EQ(receiver.init_status(), OK);
  ASSERT_EQ(wrong_receiver.init_status(), OK);

  const std::vector<uint8_t> packet = MakeRtpPacket(10, 0x55667788);
  std::vector<uint8_t> protected_packet;
  ASSERT_EQ(sender.Protect(SrtpPacketType::kRtp, packet, &protected_packet),
            OK);

  std::vector<uint8_t> tampered_packet = protected_packet;
  tampered_packet.back() ^= 0x01;
  std::vector<uint8_t> output;
  EXPECT_EQ(receiver.Unprotect(SrtpPacketType::kRtp, tampered_packet, &output),
            PERMISSION_DENIED);
  EXPECT_TRUE(output.empty());
  EXPECT_EQ(
      wrong_receiver.Unprotect(SrtpPacketType::kRtp, protected_packet, &output),
      PERMISSION_DENIED);

  ASSERT_EQ(receiver.Unprotect(SrtpPacketType::kRtp, protected_packet, &output),
            OK);
  EXPECT_EQ(output, packet);
  EXPECT_EQ(receiver.Unprotect(SrtpPacketType::kRtp, protected_packet, &output),
            ALREADY_EXISTS);
}

TEST(SrtpSessionTest, ReservesTrailerCapacityAndValidatesArguments) {
  const SrtpKey key = MakeKey(0x40);
  SrtpSession session(MakeConfig(key, key));
  ASSERT_EQ(session.init_status(), OK);

  const std::vector<uint8_t> rtp_packet = MakeRtpPacket(1, 0x12345678);
  std::vector<uint8_t> protected_rtp;
  ASSERT_EQ(session.Protect(SrtpPacketType::kRtp, rtp_packet, &protected_rtp),
            OK);
  EXPECT_GT(protected_rtp.size(), rtp_packet.size());
  EXPECT_LE(protected_rtp.size(),
            rtp_packet.size() + SrtpSession::kMaximumRtpOverhead);

  const std::vector<uint8_t> rtcp_packet = MakeRtcpPacket(0x12345678);
  std::vector<uint8_t> protected_rtcp;
  ASSERT_EQ(
      session.Protect(SrtpPacketType::kRtcp, rtcp_packet, &protected_rtcp), OK);
  EXPECT_GT(protected_rtcp.size(), rtcp_packet.size());
  EXPECT_LE(protected_rtcp.size(),
            rtcp_packet.size() + SrtpSession::kMaximumRtcpOverhead);

  EXPECT_EQ(session.Protect(SrtpPacketType::kRtp, {}, &protected_rtp),
            BAD_VALUE);
  EXPECT_EQ(session.Protect(SrtpPacketType::kRtp, rtp_packet, nullptr),
            BAD_VALUE);
  EXPECT_EQ(session.Unprotect(SrtpPacketType::kRtp, {}, &protected_rtp),
            BAD_VALUE);
}

TEST(SrtpSessionTest, SupportsRepeatedGlobalInitializationAndDestruction) {
  const SrtpKey key = MakeKey(0x50);
  for (uint16_t iteration = 0; iteration < 8; ++iteration) {
    auto sender = std::make_unique<SrtpSession>(MakeConfig(key, key));
    auto receiver = std::make_unique<SrtpSession>(MakeConfig(key, key));
    ASSERT_EQ(sender->init_status(), OK);
    ASSERT_EQ(receiver->init_status(), OK);

    const std::vector<uint8_t> packet = MakeRtpPacket(iteration, 0x76543210);
    std::vector<uint8_t> protected_packet;
    std::vector<uint8_t> received_packet;
    ASSERT_EQ(sender->Protect(SrtpPacketType::kRtp, packet, &protected_packet),
              OK);
    ASSERT_EQ(receiver->Unprotect(SrtpPacketType::kRtp, protected_packet,
                                  &received_packet),
              OK);
    EXPECT_EQ(received_packet, packet);
  }
}

}  // namespace
}  // namespace ave::media::rtp_session
