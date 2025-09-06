/*
 * message_unittest.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "../message.h"

#include <gtest/gtest.h>
#include <memory>

namespace ave {
namespace media {

class MessageTest : public ::testing::Test {
 protected:
  void SetUp() override { message_ = std::make_shared<Message>(); }

  std::shared_ptr<Message> message_;
};

TEST_F(MessageTest, SetAndFindInt32) {
  const char* name = "int32_value";
  int32_t expected = 12345;

  message_->setInt32(name, expected);
  EXPECT_TRUE(message_->contains(name));

  int32_t actual = 0;
  EXPECT_TRUE(message_->findInt32(name, &actual));
  EXPECT_EQ(expected, actual);

  // Test not found
  int32_t not_found = 0;
  EXPECT_FALSE(message_->findInt32("not_exist", &not_found));
}

TEST_F(MessageTest, SetAndFindInt64) {
  const char* name = "int64_value";
  int64_t expected = 9876543210LL;

  message_->setInt64(name, expected);
  EXPECT_TRUE(message_->contains(name));

  int64_t actual = 0;
  EXPECT_TRUE(message_->findInt64(name, &actual));
  EXPECT_EQ(expected, actual);

  // Test not found
  int64_t not_found = 0;
  EXPECT_FALSE(message_->findInt64("not_exist", &not_found));
}

TEST_F(MessageTest, SetAndFindSize) {
  const char* name = "size_value";
  size_t expected = 1024;

  message_->setSize(name, expected);
  EXPECT_TRUE(message_->contains(name));

  size_t actual = 0;
  EXPECT_TRUE(message_->findSize(name, &actual));
  EXPECT_EQ(expected, actual);

  // Test not found
  size_t not_found = 0;
  EXPECT_FALSE(message_->findSize("not_exist", &not_found));
}

TEST_F(MessageTest, SetAndFindFloat) {
  const char* name = "float_value";
  float expected = 3.14159f;

  message_->setFloat(name, expected);
  EXPECT_TRUE(message_->contains(name));

  float actual = 0.0f;
  EXPECT_TRUE(message_->findFloat(name, &actual));
  EXPECT_FLOAT_EQ(expected, actual);

  // Test not found
  float not_found = 0.0f;
  EXPECT_FALSE(message_->findFloat("not_exist", &not_found));
}

TEST_F(MessageTest, SetAndFindDouble) {
  const char* name = "double_value";
  double expected = 2.718281828;

  message_->setDouble(name, expected);
  EXPECT_TRUE(message_->contains(name));

  double actual = 0.0;
  EXPECT_TRUE(message_->findDouble(name, &actual));
  EXPECT_DOUBLE_EQ(expected, actual);

  // Test not found
  double not_found = 0.0;
  EXPECT_FALSE(message_->findDouble("not_exist", &not_found));
}

TEST_F(MessageTest, SetAndFindPointer) {
  const char* name = "pointer_value";
  int dummy = 42;
  void* expected = &dummy;

  message_->setPointer(name, expected);
  EXPECT_TRUE(message_->contains(name));

  void* actual = nullptr;
  EXPECT_TRUE(message_->findPointer(name, &actual));
  EXPECT_EQ(expected, actual);

  // Test not found
  void* not_found = nullptr;
  EXPECT_FALSE(message_->findPointer("not_exist", &not_found));
}

TEST_F(MessageTest, SetAndFindStringCStr) {
  const char* name = "string_value";
  const char* expected = "Hello, World!";

  message_->setString(name, expected);
  EXPECT_TRUE(message_->contains(name));

  std::string actual;
  EXPECT_TRUE(message_->findString(name, actual));
  EXPECT_EQ(std::string(expected), actual);

  // Test not found
  std::string not_found;
  EXPECT_FALSE(message_->findString("not_exist", not_found));
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

  // Test not found
  std::shared_ptr<Message> not_found;
  EXPECT_FALSE(message_->findMessage("not_exist", not_found));
}

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

  // Test not found
  int32_t not_found_l = 0, not_found_t = 0, not_found_r = 0, not_found_b = 0;
  EXPECT_FALSE(message_->findRect("not_exist", &not_found_l, &not_found_t,
                                  &not_found_r, &not_found_b));
}

TEST_F(MessageTest, SetAndFindObject) {
  const char* name = "object_value";

  // Test with int
  int expected_int = 42;
  message_->setObject(name, expected_int);
  EXPECT_TRUE(message_->contains(name));

  int actual_int = 0;
  EXPECT_TRUE(message_->findObject(name, actual_int));
  EXPECT_EQ(expected_int, actual_int);

  // Test with std::string
  const char* name2 = "string_object";
  std::string expected_str = "Object String";
  message_->setObject(name2, expected_str);

  std::string actual_str;
  EXPECT_TRUE(message_->findObject(name2, actual_str));
  EXPECT_EQ(expected_str, actual_str);

  // Test wrong type
  float wrong_type = 0.0f;
  EXPECT_FALSE(message_->findObject(name, wrong_type));

  // Test not found
  int not_found = 0;
  EXPECT_FALSE(message_->findObject("not_exist", not_found));
}

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

TEST_F(MessageTest, Clear) {
  message_->setInt32("value1", 100);
  message_->setString("value2", "test");

  EXPECT_TRUE(message_->contains("value1"));
  EXPECT_TRUE(message_->contains("value2"));

  message_->clear();

  EXPECT_FALSE(message_->contains("value1"));
  EXPECT_FALSE(message_->contains("value2"));
}

}  // namespace media
}  // namespace ave
