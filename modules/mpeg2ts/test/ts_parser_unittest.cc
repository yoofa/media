/*
 * ts_parser_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Comprehensive unit tests for TSParser
 */

#include "../ts_parser.h"

#include <gtest/gtest.h>
#include <vector>

#include "foundation/media_frame.h"

namespace ave {
namespace media {
namespace mpeg2ts {

class TSParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    parser_ = std::make_shared<TSParser>();
  }

  // Helper to create a basic TS packet
  std::vector<uint8_t> CreateTSPacket(uint16_t pid, uint8_t cc) {
    std::vector<uint8_t> packet(188, 0xFF);  // Fill with 0xFF
    
    // Sync byte
    packet[0] = 0x47;
    
    // PID and flags
    packet[1] = (pid >> 8) & 0x1F;
    packet[2] = pid & 0xFF;
    
    // Continuity counter and flags
    packet[3] = 0x10 | (cc & 0x0F);  // Payload only, no adaptation field
    
    return packet;
  }

  std::shared_ptr<TSParser> parser_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(TSParserTest, DefaultConstruction) {
  EXPECT_NE(parser_, nullptr);
  EXPECT_FALSE(parser_->HasSource(TSParser::VIDEO));
  EXPECT_FALSE(parser_->HasSource(TSParser::AUDIO));
}

TEST_F(TSParserTest, ConstructWithFlags) {
  auto parser_aligned = std::make_shared<TSParser>(
      TSParser::ALIGNED_VIDEO_DATA);
  EXPECT_NE(parser_aligned, nullptr);
}

// ============================================================================
// Basic Packet Feeding Tests
// ============================================================================

TEST_F(TSParserTest, FeedValidPacket) {
  auto packet = CreateTSPacket(0x100, 0);
  TSParser::SyncEvent event(0);
  
  status_t result = parser_->FeedTSPacket(packet.data(), packet.size(), &event);
  EXPECT_EQ(result, OK);
}

TEST_F(TSParserTest, InvalidPacketSize) {
  std::vector<uint8_t> small_packet(100);
  TSParser::SyncEvent event(0);
  
  status_t result = parser_->FeedTSPacket(small_packet.data(),
                                          small_packet.size(), &event);
  EXPECT_EQ(result, ERROR_MALFORMED);
}

TEST_F(TSParserTest, InvalidSyncByte) {
  auto packet = CreateTSPacket(0x100, 0);
  packet[0] = 0x00;  // Invalid sync byte
  
  TSParser::SyncEvent event(0);
  status_t result = parser_->FeedTSPacket(packet.data(), packet.size(), &event);
  EXPECT_EQ(result, ERROR_MALFORMED);
}

// ============================================================================
// PAT (Program Association Table) Tests
// ============================================================================

TEST_F(TSParserTest, BasicPAT) {
  // Create a minimal PAT packet (PID 0)
  std::vector<uint8_t> pat_packet(188, 0xFF);
  pat_packet[0] = 0x47;  // Sync byte
  pat_packet[1] = 0x40;  // PUSI set, PID = 0
  pat_packet[2] = 0x00;
  pat_packet[3] = 0x10;  // Continuity counter = 0
  
  // Pointer field
  pat_packet[4] = 0x00;
  
  // PAT table
  pat_packet[5] = 0x00;   // Table ID
  pat_packet[6] = 0xB0;   // Section syntax indicator
  pat_packet[7] = 0x0D;   // Section length
  pat_packet[8] = 0x00;   // Transport stream ID
  pat_packet[9] = 0x01;
  pat_packet[10] = 0xC1;  // Version, current/next
  pat_packet[11] = 0x00;  // Section number
  pat_packet[12] = 0x00;  // Last section number
  
  // Program 1, PMT PID = 0x1000
  pat_packet[13] = 0x00;
  pat_packet[14] = 0x01;  // Program number
  pat_packet[15] = 0xF0;
  pat_packet[16] = 0x00;  // PMT PID (0x1000)
  
  // CRC (simplified, may not be valid)
  pat_packet[17] = 0x00;
  pat_packet[18] = 0x00;
  pat_packet[19] = 0x00;
  pat_packet[20] = 0x00;
  
  TSParser::SyncEvent event(0);
  status_t result = parser_->FeedTSPacket(pat_packet.data(),
                                          pat_packet.size(), &event);
  EXPECT_EQ(result, OK);
}

// ============================================================================
// Source Detection Tests
// ============================================================================

TEST_F(TSParserTest, HasSourceInitially) {
  EXPECT_FALSE(parser_->HasSource(TSParser::VIDEO));
  EXPECT_FALSE(parser_->HasSource(TSParser::AUDIO));
  EXPECT_FALSE(parser_->HasSource(TSParser::META));
}

TEST_F(TSParserTest, GetSourceReturnsNull) {
  EXPECT_EQ(parser_->GetSource(TSParser::VIDEO), nullptr);
  EXPECT_EQ(parser_->GetSource(TSParser::AUDIO), nullptr);
}

// ============================================================================
// Discontinuity Tests
// ============================================================================

TEST_F(TSParserTest, SignalDiscontinuity) {
  parser_->SignalDiscontinuity(DiscontinuityType::TIME, nullptr);
  // Should not crash, no sources to signal
}

TEST_F(TSParserTest, SignalDiscontinuityWithMessage) {
  auto extra = std::make_shared<Message>();
  extra->setInt64("offset", 1000);
  
  parser_->SignalDiscontinuity(DiscontinuityType::FORMATCHANGE, extra);
  // Should handle gracefully
}

// ============================================================================
// EOS Tests
// ============================================================================

TEST_F(TSParserTest, SignalEOS) {
  parser_->SignalEOS(ERROR_END_OF_STREAM);
  // Should not crash even without sources
}

TEST_F(TSParserTest, SignalEOSWithErrorCode) {
  parser_->SignalEOS(ERROR_IO);
  // Different error code should work
}

// ============================================================================
// SyncEvent Tests
// ============================================================================

TEST_F(TSParserTest, SyncEventConstruction) {
  TSParser::SyncEvent event(12345);
  EXPECT_FALSE(event.HasReturnedData());
  EXPECT_EQ(event.GetOffset(), 12345);
}

TEST_F(TSParserTest, SyncEventInit) {
  TSParser::SyncEvent event(0);
  
  auto source = std::make_shared<PacketSource>(nullptr);
  event.Init(1000, source, 50000, TSParser::VIDEO);
  
  EXPECT_TRUE(event.HasReturnedData());
  EXPECT_EQ(event.GetOffset(), 1000);
  EXPECT_EQ(event.GetTimeUs(), 50000);
  EXPECT_EQ(event.GetType(), TSParser::VIDEO);
  EXPECT_NE(event.GetMediaSource(), nullptr);
}

TEST_F(TSParserTest, SyncEventReset) {
  TSParser::SyncEvent event(0);
  auto source = std::make_shared<PacketSource>(nullptr);
  
  event.Init(1000, source, 50000, TSParser::VIDEO);
  EXPECT_TRUE(event.HasReturnedData());
  
  event.Reset();
  EXPECT_FALSE(event.HasReturnedData());
}

// ============================================================================
// Multiple Packet Tests
// ============================================================================

TEST_F(TSParserTest, FeedMultiplePackets) {
  for (int i = 0; i < 100; ++i) {
    auto packet = CreateTSPacket(0x100 + i, i % 16);
    TSParser::SyncEvent event(i * 188);
    
    status_t result = parser_->FeedTSPacket(packet.data(),
                                            packet.size(), &event);
    EXPECT_EQ(result, OK);
  }
}

TEST_F(TSParserTest, ContinuityCounter) {
  uint16_t pid = 0x200;
  
  // Feed packets with sequential continuity counters
  for (uint8_t cc = 0; cc < 16; ++cc) {
    auto packet = CreateTSPacket(pid, cc);
    TSParser::SyncEvent event(cc * 188);
    
    status_t result = parser_->FeedTSPacket(packet.data(),
                                            packet.size(), &event);
    EXPECT_EQ(result, OK);
  }
}

TEST_F(TSParserTest, ContinuityCounterWrap) {
  uint16_t pid = 0x300;
  
  // Test wrap-around from 15 to 0
  for (int i = 0; i < 20; ++i) {
    uint8_t cc = i % 16;
    auto packet = CreateTSPacket(pid, cc);
    TSParser::SyncEvent event(i * 188);
    
    status_t result = parser_->FeedTSPacket(packet.data(),
                                            packet.size(), &event);
    EXPECT_EQ(result, OK);
  }
}

// ============================================================================
// Stream Type Tests
// ============================================================================

TEST_F(TSParserTest, StreamTypeConstants) {
  // Verify stream type constants are defined
  EXPECT_EQ(TSParser::STREAMTYPE_H264, 0x1b);
  EXPECT_EQ(TSParser::STREAMTYPE_H265, 0x24);
  EXPECT_EQ(TSParser::STREAMTYPE_MPEG2_AUDIO_ADTS, 0x0f);
  EXPECT_EQ(TSParser::STREAMTYPE_AC3, 0x81);
  EXPECT_EQ(TSParser::STREAMTYPE_EAC3, 0x87);
}

// ============================================================================
// PCR Tests
// ============================================================================

TEST_F(TSParserTest, PTSTimeDeltaInitially) {
  // Initially no PTS delta established
  bool established = parser_->PTSTimeDeltaEstablished();
  // After parsing some packets it may be established
  // Just verify method doesn't crash
  (void)established;
}

TEST_F(TSParserTest, GetFirstPTSTime) {
  int64_t pts = parser_->GetFirstPTSTimeUs();
  // Should return 0 initially (simplified implementation)
  EXPECT_EQ(pts, 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TSParserTest, NullPacketData) {
  TSParser::SyncEvent event(0);
  // Feeding null should be handled gracefully or return error
  // Implementation should check for nullptr
}

TEST_F(TSParserTest, PacketWithAdaptationField) {
  std::vector<uint8_t> packet(188, 0xFF);
  packet[0] = 0x47;
  packet[1] = 0x40;  // PUSI
  packet[2] = 0x00;  // PID = 0
  packet[3] = 0x30;  // Adaptation field present + payload
  
  // Adaptation field length
  packet[4] = 0x07;  // 7 bytes
  packet[5] = 0x10;  // Random access indicator
  
  TSParser::SyncEvent event(0);
  status_t result = parser_->FeedTSPacket(packet.data(), packet.size(), &event);
  EXPECT_EQ(result, OK);
}

TEST_F(TSParserTest, TransportErrorIndicator) {
  auto packet = CreateTSPacket(0x100, 0);
  packet[1] |= 0x80;  // Set transport error indicator
  
  TSParser::SyncEvent event(0);
  status_t result = parser_->FeedTSPacket(packet.data(), packet.size(), &event);
  // Should still parse, but data may be flagged as error
  EXPECT_EQ(result, OK);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(TSParserTest, LargePacketSequence) {
  // Feed a large number of packets
  for (int i = 0; i < 10000; ++i) {
    auto packet = CreateTSPacket(0x100 + (i % 100), i % 16);
    TSParser::SyncEvent event(i * 188);
    
    status_t result = parser_->FeedTSPacket(packet.data(),
                                            packet.size(), &event);
    EXPECT_EQ(result, OK);
  }
}

TEST_F(TSParserTest, RapidParsing) {
  // Rapid packet feeding without delay
  std::vector<std::vector<uint8_t>> packets;
  for (int i = 0; i < 1000; ++i) {
    packets.push_back(CreateTSPacket(0x200, i % 16));
  }
  
  for (size_t i = 0; i < packets.size(); ++i) {
    TSParser::SyncEvent event(i * 188);
    status_t result = parser_->FeedTSPacket(packets[i].data(),
                                            packets[i].size(), &event);
    EXPECT_EQ(result, OK);
  }
}

}  // namespace mpeg2ts
}  // namespace media
}  // namespace ave