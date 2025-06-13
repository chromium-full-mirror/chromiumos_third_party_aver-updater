// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <base/files/file.h>
#include <base/files/file_util.h>

#include "utilities.h"

namespace {

class UtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(UtilsTest, ConvertHexStringToIntReturnTrue) {
  int value;
  EXPECT_TRUE(ConvertHexStringToInt("910", &value));
  EXPECT_EQ(2320, value);
  EXPECT_TRUE(ConvertHexStringToInt("970", &value));
  EXPECT_EQ(2416, value);
  EXPECT_TRUE(ConvertHexStringToInt("980", &value));
  EXPECT_EQ(2432, value);
  EXPECT_TRUE(ConvertHexStringToInt("9a0", &value));
  EXPECT_EQ(2464, value);
}

TEST_F(UtilsTest, ConvertHexStringToIntReturnFalse) {
  int value;
  EXPECT_FALSE(ConvertHexStringToInt("", &value));
  EXPECT_FALSE(ConvertHexStringToInt("foo", &value));
}

// TODO(b/274372995): WriteFile assertion fails.
TEST_F(UtilsTest, DISABLED_ReadFileContentReturnTrueAndStringEq) {
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateNewTempDirectory("aver_test_tmp_dir", &file_path));

  base::FilePath test_file = file_path.Append("test.txt");
  ASSERT_TRUE(base::WriteFile(test_file, "test"));

  std::string file_content;
  EXPECT_TRUE(ReadFileContent(test_file, &file_content));
  EXPECT_EQ(0, file_content.compare("test"));
  ASSERT_TRUE(base::DeletePathRecursively(file_path));
}

TEST_F(UtilsTest, ReadFileContentReturnFalse) {
  base::FilePath empty_path;
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateNewTempDirectory("aver_test_tmp_dir", &file_path));

  base::FilePath test_file = file_path.Append("test.txt");

  std::string file_content;
  EXPECT_FALSE(ReadFileContent(empty_path, &file_content));
  EXPECT_FALSE(ReadFileContent(test_file, &file_content));
  ASSERT_TRUE(base::DeletePathRecursively(file_path));
}

TEST_F(UtilsTest, VerifyDeviceResponseReturnTrue) {
  int i;
  std::vector<char> msg;
  std::string gold_sample = "gold";
  for (i = 0; i < gold_sample.length(); i++)
    msg.push_back(gold_sample.c_str()[i]);
  EXPECT_TRUE(VerifyDeviceResponse(msg, gold_sample, 0));

  std::string compare_str1 = " gold";
  msg.clear();
  for (i = 0; i < compare_str1.length(); i++)
    msg.push_back(compare_str1.c_str()[i]);
  EXPECT_TRUE(VerifyDeviceResponse(msg, gold_sample, 1));

  std::string compare_str2 = "  gold";
  msg.clear();
  for (i = 0; i < compare_str2.length(); i++)
    msg.push_back(compare_str2.c_str()[i]);
  EXPECT_TRUE(VerifyDeviceResponse(msg, gold_sample, 2));

  std::string compare_str3 = "   gold";
  msg.clear();
  for (i = 0; i < compare_str3.length(); i++)
    msg.push_back(compare_str3.c_str()[i]);
  EXPECT_TRUE(VerifyDeviceResponse(msg, gold_sample, 3));
}

TEST_F(UtilsTest, VerifyDeviceResponseReturnFalse) {
  int i;
  std::vector<char> msg;
  std::string gold_sample = "gold";
  std::string err_sample = "gol";
  for (i = 0; i < err_sample.length(); i++)
    msg.push_back(err_sample.c_str()[i]);
  EXPECT_FALSE(VerifyDeviceResponse(msg, gold_sample, 0));

  for (i = 0; i < gold_sample.length(); i++)
    msg.push_back(gold_sample.c_str()[i]);
  EXPECT_FALSE(VerifyDeviceResponse(msg, gold_sample, 1));
}

TEST_F(UtilsTest, CompareVersionsReturnZero) {
    EXPECT_EQ(0, CompareVersions("0.0.1000.22", "0.0.1000.22"));
    EXPECT_EQ(0, CompareVersions("0.0.6002.58", "0.0.6002.58"));
    EXPECT_EQ(0, CompareVersions("0.0.0018.20", "0.0.0018.20"));
    EXPECT_EQ(0, CompareVersions("9.9.9999.99", "9.9.9999.99"));
    EXPECT_EQ(0, CompareVersions("0.0.0000.00", "0.0.0000.00"));
}

TEST_F(UtilsTest, CompareVersionsReturnPositive) {
    EXPECT_EQ(1000, CompareVersions("0.0.1000.22", ""));
    EXPECT_EQ(5, CompareVersions("5.0.1000.22", "0.0.1000.22"));
    EXPECT_EQ(4, CompareVersions("0.4.1000.22", "0.0.1000.22"));
    EXPECT_EQ(234, CompareVersions("0.0.1234.22", "0.0.1000.22"));
}

TEST_F(UtilsTest, CompareVersionsReturnNegative) {
    EXPECT_EQ(-1000, CompareVersions("", "0.0.1000.22"));
    EXPECT_EQ(-2, CompareVersions("0.0.1000.20", "0.0.1000.22"));
    EXPECT_EQ(-3, CompareVersions("0.0.6002.55", "0.0.6002.58"));
    EXPECT_EQ(-1, CompareVersions("0.0.0018.19", "0.0.0018.20"));
}


}  // namespace
