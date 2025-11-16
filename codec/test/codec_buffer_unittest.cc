/*
 * codec_buffer_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "media/codec/codec_buffer.h"

#include <cstring>

#include "test/gtest.h"

namespace ave {
namespace media {

TEST(CodecBufferTest, ConstructWithCapacity) {
  CodecBuffer buffer(1024);
  EXPECT_EQ(buffer.capacity(), 1024u);
  EXPECT_NE(buffer.base(), nullptr);
  EXPECT_NE(buffer.data(), nullptr);
  EXPECT_EQ(buffer.buffer_type(), CodecBuffer::BufferType::kTypeNormal);
}

TEST(CodecBufferTest, ConstructWithData) {
  uint8_t data[256];
  std::memset(data, 0xAB, sizeof(data));
  CodecBuffer buffer(data, sizeof(data));
  EXPECT_EQ(buffer.capacity(), 256u);
  EXPECT_NE(buffer.base(), nullptr);
}

TEST(CodecBufferTest, SetRange) {
  CodecBuffer buffer(1024);
  EXPECT_EQ(buffer.SetRange(10, 100), OK);
  EXPECT_EQ(buffer.offset(), 10u);
  EXPECT_EQ(buffer.size(), 100u);
}

TEST(CodecBufferTest, EnsureCapacity) {
  CodecBuffer buffer(64);
  EXPECT_EQ(buffer.capacity(), 64u);
  EXPECT_EQ(buffer.EnsureCapacity(256, false), OK);
  EXPECT_GE(buffer.capacity(), 256u);
}

TEST(CodecBufferTest, EnsureCapacityWithCopy) {
  CodecBuffer buffer(64);
  // Write some data
  std::memset(buffer.data(), 0xCD, 64);
  buffer.SetRange(0, 64);

  EXPECT_EQ(buffer.EnsureCapacity(256, true), OK);
  EXPECT_GE(buffer.capacity(), 256u);
  // Data should be preserved
  EXPECT_EQ(buffer.data()[0], 0xCD);
  EXPECT_EQ(buffer.data()[63], 0xCD);
}

TEST(CodecBufferTest, TextureId) {
  CodecBuffer buffer(64);
  EXPECT_EQ(buffer.buffer_type(), CodecBuffer::BufferType::kTypeNormal);
  EXPECT_EQ(buffer.texture_id(), -1);

  buffer.SetTextureId(42);
  EXPECT_EQ(buffer.texture_id(), 42);
  EXPECT_EQ(buffer.buffer_type(), CodecBuffer::BufferType::kTypeTexture);
}

TEST(CodecBufferTest, NativeHandle) {
  CodecBuffer buffer(64);
  EXPECT_EQ(buffer.buffer_type(), CodecBuffer::BufferType::kTypeNormal);
  EXPECT_EQ(buffer.native_handle(), nullptr);

  int dummy = 123;
  buffer.SetNativeHandle(&dummy);
  EXPECT_EQ(buffer.native_handle(), &dummy);
  EXPECT_EQ(buffer.buffer_type(), CodecBuffer::BufferType::kTypeNativeHandle);
}

TEST(CodecBufferTest, Format) {
  CodecBuffer buffer(64);
  auto& format = buffer.format();
  EXPECT_NE(format, nullptr);
}

TEST(CodecBufferTest, ResetBuffer) {
  CodecBuffer buffer(64);
  auto new_buf = std::make_shared<media::Buffer>(128);
  EXPECT_EQ(buffer.ResetBuffer(new_buf), OK);
  EXPECT_EQ(buffer.capacity(), 128u);
}

TEST(CodecBufferTest, WriteAndReadData) {
  CodecBuffer buffer(256);
  const char* test_data = "Hello, CodecBuffer!";
  size_t len = std::strlen(test_data);

  std::memcpy(buffer.data(), test_data, len);
  buffer.SetRange(0, len);

  EXPECT_EQ(buffer.size(), len);
  EXPECT_EQ(std::memcmp(buffer.data(), test_data, len), 0);
}

}  // namespace media
}  // namespace ave
