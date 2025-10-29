/*
 * message_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "../message.h"

#include <gtest/gtest.h>
#include <memory>

#include "../buffer.h"
#include "../media_meta.h"

namespace ave {
namespace media {

class MessageTest : public ::testing::Test {
 protected:
  void SetUp() override { message_ = std::make_shared<Message>(); }

  std::shared_ptr<Message> message_;
};

// ============================================================================
// Basic Construction and Properties Tests
// ============================================================================

TEST_F(MessageTest, DefaultConstruction) {
  EXPECT_EQ(message_->what(), 0u);
  EXPECT_FALSE(message_->contains("nonexistent"));
}

TEST_F(MessageTest, ConstructionWithWhat) {
  auto msg = std::make_shared<Message>(42, nullptr);
  EXPECT_EQ(msg->what(), 42u);
}

TEST_F(MessageTest, SetAndGetWhat) {
  message_->setWhat(123);
  EXPECT_EQ(message_->what(), 123u);
  
  message_->setWhat(456);
  EXPECT_EQ(message_->what(), 456u);
}

// ============================================================================
// Int32 Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindInt32) {
  const char* name = "int32_value";
  int32_t expected = 12345;

  message_->setInt32(name, expected);
  EXPECT_TRUE(message_->contains(name));

  int32_t actual = 0;
  EXPECT_TRUE(message_->findInt32(name, &actual));
  EXPECT_EQ(expected, actual);
}

TEST_F(MessageTest, FindInt32NotFound) {
  int32_t value = 999;
  EXPECT_FALSE(message_->findInt32("not_exist", &value));
  EXPECT_EQ(value, 999);  // Value should remain unchanged
}

TEST_F(MessageTest, Int32Boundaries) {
  message_->setInt32("min", std::numeric_limits<int32_t>::min());
  message_->setInt32("max", std::numeric_limits<int32_t>::max());
  message_->setInt32("zero", 0);
  message_->setInt32("negative", -12345);

  int32_t min, max, zero, neg;
  EXPECT_TRUE(message_->findInt32("min", &min));
  EXPECT_TRUE(message_->findInt32("max", &max));
  EXPECT_TRUE(message_->findInt32("zero", &zero));
  EXPECT_TRUE(message_->findInt32("negative", &neg));

  EXPECT_EQ(min, std::numeric_limits<int32_t>::min());
  EXPECT_EQ(max, std::numeric_limits<int32_t>::max());
  EXPECT_EQ(zero, 0);
  EXPECT_EQ(neg, -12345);
}

// ============================================================================
// Int64 Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindInt64) {
  const char* name = "int64_value";
  int64_t expected = 9876543210LL;

  message_->setInt64(name, expected);
  EXPECT_TRUE(message_->contains(name));

  int64_t actual = 0;
  EXPECT_TRUE(message_->findInt64(name, &actual));
  EXPECT_EQ(expected, actual);
}

TEST_F(MessageTest, Int64Boundaries) {
  message_->setInt64("min", std::numeric_limits<int64_t>::min());
  message_->setInt64("max", std::numeric_limits<int64_t>::max());

  int64_t min, max;
  EXPECT_TRUE(message_->findInt64("min", &min));
  EXPECT_TRUE(message_->findInt64("max", &max));

  EXPECT_EQ(min, std::numeric_limits<int64_t>::min());
  EXPECT_EQ(max, std::numeric_limits<int64_t>::max());
}

// ============================================================================
// Size Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindSize) {
  const char* name = "size_value";
  size_t expected = 1024;

  message_->setSize(name, expected);
  EXPECT_TRUE(message_->contains(name));

  size_t actual = 0;
  EXPECT_TRUE(message_->findSize(name, &actual));
  EXPECT_EQ(expected, actual);
}

TEST_F(MessageTest, SizeLargeValue) {
  size_t large = std::numeric_limits<size_t>::max();
  message_->setSize("large", large);

  size_t value;
  EXPECT_TRUE(message_->findSize("large", &value));
  EXPECT_EQ(value, large);
}

// ============================================================================
// Float Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindFloat) {
  const char* name = "float_value";
  float expected = 3.14159f;

  message_->setFloat(name, expected);
  EXPECT_TRUE(message_->contains(name));

  float actual = 0.0f;
  EXPECT_TRUE(message_->findFloat(name, &actual));
  EXPECT_FLOAT_EQ(expected, actual);
}

TEST_F(MessageTest, FloatSpecialValues) {
  message_->setFloat("inf", std::numeric_limits<float>::infinity());
  message_->setFloat("neg_inf", -std::numeric_limits<float>::infinity());
  message_->setFloat("zero", 0.0f);
  message_->setFloat("negative", -123.456f);

  float inf, neg_inf, zero, neg;
  EXPECT_TRUE(message_->findFloat("inf", &inf));
  EXPECT_TRUE(message_->findFloat("neg_inf", &neg_inf));
  EXPECT_TRUE(message_->findFloat("zero", &zero));
  EXPECT_TRUE(message_->findFloat("negative", &neg));

  EXPECT_TRUE(std::isinf(inf) && inf > 0);
  EXPECT_TRUE(std::isinf(neg_inf) && neg_inf < 0);
  EXPECT_FLOAT_EQ(zero, 0.0f);
  EXPECT_FLOAT_EQ(neg, -123.456f);
}

// ============================================================================
// Double Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindDouble) {
  const char* name = "double_value";
  double expected = 2.718281828;

  message_->setDouble(name, expected);
  EXPECT_TRUE(message_->contains(name));

  double actual = 0.0;
  EXPECT_TRUE(message_->findDouble(name, &actual));
  EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(MessageTest, DoublePrecision) {
  double precise = 1.23456789012345;
  message_->setDouble("precise", precise);

  double value;
  EXPECT_TRUE(message_->findDouble("precise", &value));
  EXPECT_DOUBLE_EQ(value, precise);
}

// ============================================================================
// Pointer Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindPointer) {
  const char* name = "pointer_value";
  int dummy = 42;
  void* expected = &dummy;

  message_->setPointer(name, expected);
  EXPECT_TRUE(message_->contains(name));

  void* actual = nullptr;
  EXPECT_TRUE(message_->findPointer(name, &actual));
  EXPECT_EQ(expected, actual);
}

TEST_F(MessageTest, NullPointer) {
  message_->setPointer("null", nullptr);

  void* ptr;
  EXPECT_TRUE(message_->findPointer("null", &ptr));
  EXPECT_EQ(ptr, nullptr);
}

// ============================================================================
// String Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindStringCStr) {
  const char* name = "string_value";
  const char* expected = "Hello, World!";

  message_->setString(name, expected);
  EXPECT_TRUE(message_->contains(name));

  std::string actual;
  EXPECT_TRUE(message_->findString(name, actual));
  EXPECT_EQ(std::string(expected), actual);
}

TEST_F(MessageTest, SetAndFindStringWithLength) {
  const char* name = "string_value";
  const char* input = "Hello, World!";
  ssize_t len = 5;  // Only "Hello"

  message_->setString(name, input, len);
  EXPECT_TRUE(message_->contains(name));

  std::string actual;
  EXPECT_TRUE(message_->findString(name, actual));
  EXPECT_EQ("Hello", actual);
}

TEST_F(MessageTest, SetAndFindStringStdString) {
  const char* name = "string_value";
  std::string expected = "Test String";

  message_->setString(name, expected);
  EXPECT_TRUE(message_->contains(name));

  std::string actual;
  EXPECT_TRUE(message_->findString(name, actual));
  EXPECT_EQ(expected, actual);
}

TEST_F(MessageTest, EmptyString) {
  message_->setString("empty", "");

  std::string value;
  EXPECT_TRUE(message_->findString("empty", value));
  EXPECT_TRUE(value.empty());
}

TEST_F(MessageTest, StringWithSpecialCharacters) {
  std::string special = "Line1\nLine2\tTab\0Null";
  message_->setString("special", special);

  std::string value;
  EXPECT_TRUE(message_->findString("special", value));
  EXPECT_EQ(value, special);
}

// ============================================================================
// Rect Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindRect) {
  const char* name = "rect_value";
  int32_t left = 10, top = 20, right = 100, bottom = 200;

  message_->setRect(name, left, top, right, bottom);
  EXPECT_TRUE(message_->contains(name));

  int32_t actual_left = 0, actual_top = 0, actual_right = 0, actual_bottom = 0;
  EXPECT_TRUE(message_->findRect(name, &actual_left, &actual_top, &actual_right,
                                 &actual_bottom));
  EXPECT_EQ(left, actual_left);
  EXPECT_EQ(top, actual_top);
  EXPECT_EQ(right, actual_right);
  EXPECT_EQ(bottom, actual_bottom);
}

TEST_F(MessageTest, RectWithNegativeValues) {
  message_->setRect("rect", -10, -20, 100, 200);

  int32_t l, t, r, b;
  EXPECT_TRUE(message_->findRect("rect", &l, &t, &r, &b));
  EXPECT_EQ(l, -10);
  EXPECT_EQ(t, -20);
  EXPECT_EQ(r, 100);
  EXPECT_EQ(b, 200);
}

// ============================================================================
// Message Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindMessage) {
  const char* name = "message_value";
  auto expected = std::make_shared<Message>();
  expected->setInt32("inner_value", 999);

  message_->setMessage(name, expected);
  EXPECT_TRUE(message_->contains(name));

  std::shared_ptr<Message> actual;
  EXPECT_TRUE(message_->findMessage(name, actual));
  EXPECT_EQ(expected, actual);

  int32_t inner_value = 0;
  EXPECT_TRUE(actual->findInt32("inner_value", &inner_value));
  EXPECT_EQ(999, inner_value);
}

TEST_F(MessageTest, NestedMessages) {
  auto level2 = std::make_shared<Message>();
  level2->setString("data", "deep");

  auto level1 = std::make_shared<Message>();
  level1->setMessage("nested", level2);

  message_->setMessage("root", level1);

  std::shared_ptr<Message> l1, l2;
  EXPECT_TRUE(message_->findMessage("root", l1));
  EXPECT_TRUE(l1->findMessage("nested", l2));

  std::string data;
  EXPECT_TRUE(l2->findString("data", data));
  EXPECT_EQ(data, "deep");
}

// ============================================================================
// Buffer Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindBuffer) {
  const char* name = "buffer_value";
  auto expected = std::make_shared<Buffer>(128);
  expected->setInt32Data(42);

  message_->setBuffer(name, expected);
  EXPECT_TRUE(message_->contains(name));

  std::shared_ptr<Buffer> actual;
  EXPECT_TRUE(message_->findBuffer(name, actual));
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(actual->int32Data(), 42);
}

TEST_F(MessageTest, BufferWithData) {
  uint8_t data[] = {1, 2, 3, 4, 5};
  auto buffer = Buffer::CreateAsCopy(data, sizeof(data));

  message_->setBuffer("buf", buffer);

  std::shared_ptr<Buffer> retrieved;
  EXPECT_TRUE(message_->findBuffer("buf", retrieved));
  EXPECT_EQ(retrieved->size(), sizeof(data));
  EXPECT_EQ(0, memcmp(retrieved->data(), data, sizeof(data)));
}

// ============================================================================
// MessageObject Tests
// ============================================================================

TEST_F(MessageTest, SetAndFindMessageObject) {
  const char* name = "object_value";
  auto expected = std::make_shared<MediaMeta>(MediaType::VIDEO);

  message_->setObject(name, expected);
  EXPECT_TRUE(message_->contains(name));

  std::shared_ptr<MessageObject> actual;
  EXPECT_TRUE(message_->findObject(name, actual));
  EXPECT_EQ(expected, actual);
  
  // Verify we can cast back
  auto meta = std::dynamic_pointer_cast<MediaMeta>(actual);
  EXPECT_NE(meta, nullptr);
  EXPECT_EQ(meta->stream_type(), MediaType::VIDEO);
}

// ============================================================================
// Overwrite and Clear Tests
// ============================================================================

TEST_F(MessageTest, OverwriteValue) {
  const char* name = "value";

  message_->setInt32(name, 100);
  int32_t value1 = 0;
  EXPECT_TRUE(message_->findInt32(name, &value1));
  EXPECT_EQ(100, value1);

  // Overwrite with new value
  message_->setInt32(name, 200);
  int32_t value2 = 0;
  EXPECT_TRUE(message_->findInt32(name, &value2));
  EXPECT_EQ(200, value2);
}

TEST_F(MessageTest, OverwriteDifferentTypes) {
  const char* name = "value";

  message_->setInt32(name, 123);
  EXPECT_TRUE(message_->contains(name));

  // Overwrite with different type
  message_->setString(name, "test");
  
  // Old type should not be found
  int32_t int_val;
  EXPECT_FALSE(message_->findInt32(name, &int_val));
  
  // New type should be found
  std::string str_val;
  EXPECT_TRUE(message_->findString(name, str_val));
  EXPECT_EQ(str_val, "test");
}

TEST_F(MessageTest, Clear) {
  message_->setInt32("value1", 100);
  message_->setString("value2", "test");
  message_->setFloat("value3", 1.5f);

  EXPECT_TRUE(message_->contains("value1"));
  EXPECT_TRUE(message_->contains("value2"));
  EXPECT_TRUE(message_->contains("value3"));

  message_->clear();

  EXPECT_FALSE(message_->contains("value1"));
  EXPECT_FALSE(message_->contains("value2"));
  EXPECT_FALSE(message_->contains("value3"));
}

// ============================================================================
// Multiple Values Tests
// ============================================================================

TEST_F(MessageTest, MultipleValues) {
  message_->setInt32("int_val", 123);
  message_->setFloat("float_val", 1.5f);
  message_->setString("string_val", "test");

  EXPECT_TRUE(message_->contains("int_val"));
  EXPECT_TRUE(message_->contains("float_val"));
  EXPECT_TRUE(message_->contains("string_val"));

  int32_t int_val = 0;
  float float_val = 0.0f;
  std::string string_val;

  EXPECT_TRUE(message_->findInt32("int_val", &int_val));
  EXPECT_TRUE(message_->findFloat("float_val", &float_val));
  EXPECT_TRUE(message_->findString("string_val", string_val));

  EXPECT_EQ(123, int_val);
  EXPECT_FLOAT_EQ(1.5f, float_val);
  EXPECT_EQ("test", string_val);
}

TEST_F(MessageTest, ManyValues) {
  // Test with many values to ensure no capacity issues
  for (int i = 0; i < 100; ++i) {
    std::string name = "key_" + std::to_string(i);
    message_->setInt32(name.c_str(), i);
  }

  for (int i = 0; i < 100; ++i) {
    std::string name = "key_" + std::to_string(i);
    int32_t value;
    EXPECT_TRUE(message_->findInt32(name.c_str(), &value));
    EXPECT_EQ(value, i);
  }
}

// ============================================================================
// Type Safety Tests
// ============================================================================

TEST_F(MessageTest, TypeMismatch) {
  message_->setInt32("value", 123);

  // Try to retrieve as wrong type
  float float_val;
  EXPECT_FALSE(message_->findFloat("value", &float_val));

  std::string string_val;
  EXPECT_FALSE(message_->findString("value", string_val));

  // Correct type should work
  int32_t int_val;
  EXPECT_TRUE(message_->findInt32("value", &int_val));
  EXPECT_EQ(int_val, 123);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(MessageTest, EmptyName) {
  message_->setInt32("", 42);
  
  int32_t value;
  EXPECT_TRUE(message_->findInt32("", &value));
  EXPECT_EQ(value, 42);
}

TEST_F(MessageTest, LongName) {
  std::string long_name(1000, 'a');
  message_->setInt32(long_name.c_str(), 123);

  int32_t value;
  EXPECT_TRUE(message_->findInt32(long_name.c_str(), &value));
  EXPECT_EQ(value, 123);
}

TEST_F(MessageTest, ContainsCheck) {
  EXPECT_FALSE(message_->contains("nonexistent"));

  message_->setInt32("exists", 42);
  EXPECT_TRUE(message_->contains("exists"));
  EXPECT_FALSE(message_->contains("still_nonexistent"));
}

// ============================================================================
// Dup Tests
// ============================================================================

TEST_F(MessageTest, Dup) {
  message_->setWhat(42);
  message_->setInt32("value", 123);

  auto duplicated = message_->dup();
  EXPECT_NE(duplicated, nullptr);
  EXPECT_EQ(duplicated->what(), 42u);
}

}  // namespace media
}  // namespace ave