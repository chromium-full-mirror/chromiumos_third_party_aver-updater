// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utilities.h"

#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <iostream>

#include <base/files/dir_reader_posix.h>
#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {
constexpr unsigned int kDefaultVersionComponentCount = 4;
constexpr unsigned int kReceiveDataStartPosition = 3;
const char kAverLockFile[] = "/tmp/aver-updater.lock";

std::vector<std::string> SplitString(std::string string,
                                    std::string delimiters) {
  return base::SplitString(string, delimiters, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
}
}  // namespace

bool GetDirectoryContents(std::string directory,
                          std::vector<std::string>* contents) {
  base::DirReaderPosix reader(directory.c_str());
  if (!reader.IsValid())
    return false;

  while (reader.Next())
    (*contents).push_back(std::string(reader.name()));
  return true;
}

bool ReadFirmwareFileToBuffer(base::FilePath file_path, std::vector<char> *buffer) {
  int64_t size_of_file;
  if (!base::GetFileSize( file_path, &size_of_file)) {
    LOG(ERROR) << "Can't get firmware file size.";
    return false;
  }

  std::vector<char> buf(size_of_file);
  if (base::ReadFile(file_path, buf.data(), size_of_file) < 0) {
    LOG(ERROR) << "Read firmware file to buffer failed.";
    return false;
  }

  *buffer = buf;
  return true;
}

int CompareVersions(std::string version1, std::string version2) {
  std::string dev_ver = version1.substr(0, version1.find_first_not_of("0123456789."));
  std::string img_ver = version2.substr(0, version2.find_first_not_of("0123456789."));
  std::vector<std::string> first_tokens = SplitString(dev_ver, ".");
  std::vector<std::string> second_tokens = SplitString(img_ver, ".");

  while (first_tokens.size() < kDefaultVersionComponentCount)
    first_tokens.push_back("0");
  while (second_tokens.size() < kDefaultVersionComponentCount)
    second_tokens.push_back("0");

  for (unsigned int i = 0; i < kDefaultVersionComponentCount; i++) {
    int first_version = 0;
    int second_version = 0;
    if (!(base::StringToInt(first_tokens[i], &first_version))) {
      if (first_version == 0)
        return 1;
    }
    if (!(base::StringToInt(second_tokens[i], &second_version))) {
      if (second_version == 0)
        return 1;
    }
    if (first_version > second_version)
      return 1;
    if (first_version < second_version)
      return -1;
  }
  return 0;
}

bool LockUpdater() {
  int lockfile = open(kAverLockFile, O_WRONLY | O_CREAT, 0664);
  if (lockfile < 0)
    return false;

  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;
  // When the process runs, the file is locked.
  // And release the lock when the process exits.
  // The next process can open file and lock it again.
  return fcntl(lockfile, F_SETLK, &fl) == 0;
}

bool ReadFileContent(std::string filepath, std::string* output) {
  if (filepath.empty())
    return false;

  base::File::Info info;
  base::FilePath input_file_path(filepath.c_str());
  GetFileInfo(input_file_path, &info);
  if (info.size <= 0)
    return false;

  char* buffer = new char[info.size]();
  bool result = ReadFile(input_file_path, buffer, info.size);

  *output = buffer;
  delete[] buffer;
  return result;
}

bool ConvertHexStringToInt(std::string hex_string, int* output_value) {
  if (hex_string.empty())
    return false;
  std::stringstream string_stream;
  string_stream << std::hex << hex_string;
  unsigned int hex_number;
  string_stream >> hex_number;
  if (output_value)
    *output_value = hex_number;
  return true;
}

bool VerifyDeviceResponse(std::vector<char> hid_return_msg,
                                std::string isp_progress_word) {
  if (hid_return_msg.size() < isp_progress_word.size() + kReceiveDataStartPosition) {
    LOG(ERROR) << "The hid_return_msg_ size is too small:" << hid_return_msg.size();
    return false;
  }

  if (std::equal(isp_progress_word.begin(), isp_progress_word.end(),
      hid_return_msg.begin() + kReceiveDataStartPosition))
    return true;
  else
    return false;
}