/*
 * Copyright (C) 2026 The aspect-oriented-express Authors.
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/audio/pcm_ring_buffer.h"

#include <array>
#include <cstdint>

#include "test/gtest.h"

namespace ave::media {
namespace {

TEST(PcmRingBufferTest, WrapsAndPreservesByteOrder) {
  PcmRingBuffer ring(6);
  constexpr std::array<uint8_t, 4> kFirst = {1, 2, 3, 4};
  constexpr std::array<uint8_t, 4> kSecond = {5, 6, 7, 8};
  std::array<uint8_t, 3> first_output{};
  std::array<uint8_t, 5> second_output{};

  EXPECT_EQ(ring.Write(kFirst), 4U);
  EXPECT_EQ(ring.Read(first_output), 3U);
  EXPECT_EQ(first_output, (std::array<uint8_t, 3>{1, 2, 3}));
  EXPECT_EQ(ring.Write(kSecond), 4U);
  EXPECT_EQ(ring.Read(second_output), 5U);
  EXPECT_EQ(second_output, (std::array<uint8_t, 5>{4, 5, 6, 7, 8}));
  EXPECT_EQ(ring.queued_bytes(), 0U);
}

TEST(PcmRingBufferTest, RejectsWholeWriteWhenCapacityWouldOverflow) {
  PcmRingBuffer ring(4);
  constexpr std::array<uint8_t, 3> kFirst = {1, 2, 3};
  constexpr std::array<uint8_t, 2> kOverflow = {4, 5};
  std::array<uint8_t, 4> output{};

  EXPECT_EQ(ring.Write(kFirst), 3U);
  EXPECT_EQ(ring.Write(kOverflow), 0U);
  EXPECT_EQ(ring.Read(output), 3U);
  EXPECT_EQ(output[0], 1U);
  EXPECT_EQ(output[1], 2U);
  EXPECT_EQ(output[2], 3U);
}

}  // namespace
}  // namespace ave::media
