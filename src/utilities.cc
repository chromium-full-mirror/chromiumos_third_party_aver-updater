// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utilities.h"

#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include <archive.h>
#include <archive_entry.h>

#include <base/files/dir_reader_posix.h>
#include <base/files/file.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {
const char kAverLockFile[] = "/run/lock/aver-updater.lock";
const char kVersionNumberTokens[] = "0123456789.";
const char kHexNumberTokens[] = "0123456789abcdef\n";

constexpr unsigned int kDefaultVersionComponentCount = 4;

std::vector<std::string> SplitString(std::string string,
                                     std::string delimiters) {
  return base::SplitString(string, delimiters, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}
}  // namespace

bool GetDirectoryContents(const std::string& directory,
                          std::vector<std::string>* contents) {
  base::DirReaderPosix reader(directory.c_str());
  if (!reader.IsValid())
    return false;

  while (reader.Next())
    (*contents).push_back(std::string(reader.name()));
  return true;
}

bool FindIntermediateCompressedFile(const base::FilePath& folder,
                                    const std::string& firmware_name) {
  int types = base::FileEnumerator::FILES;
  base::FileEnumerator file_enum(folder, false, types, firmware_name);
  base::FilePath result = file_enum.Next();
  if (result.empty())
    return false;
  return true;
}

bool ReadFirmwareFileToBuffer(const base::FilePath& file_path,
                              std::vector<char>* buffer) {
  std::optional<int64_t> raw_size = base::GetFileSize(file_path);
  if (!raw_size.has_value()) {
    LOG(ERROR) << "Can't get firmware file size.";
    return false;
  }

  int64_t size_of_file = raw_size.value();
  std::vector<char> buf(size_of_file);
  if (base::ReadFile(file_path, buf.data(), size_of_file) < 0) {
    LOG(ERROR) << "Read firmware file to buffer failed.";
    return false;
  }

  *buffer = buf;
  return true;
}

int CompareVersions(const std::string& version1, const std::string& version2) {
  std::string dev_ver =
    version1.substr(0, version1.find_first_not_of(kVersionNumberTokens));
  std::string img_ver =
    version2.substr(0, version2.find_first_not_of(kVersionNumberTokens));
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

    if (first_version != second_version) {
      return first_version - second_version;
    }
  }
  return 0;
}

bool LockUpdater() {
  int lock = open(kAverLockFile, O_WRONLY | O_CREAT |
                  O_NOFOLLOW | O_CLOEXEC |  O_TRUNC, 0664);
  if (lock < 0)
    return false;

  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;
  // When the process runs, the file is locked.
  // And release the lock when the process exits.
  // The next process can open file and lock it again.
  return fcntl(lock, F_SETLK, &fl) == 0;
}

bool ReadFileContent(const base::FilePath& input_file_path,
                     std::string* output) {
  if (input_file_path.empty())
    return false;

  base::File::Info info;
  GetFileInfo(input_file_path, &info);
  if (info.size <= 0)
    return false;

  char* buffer = new char[info.size]();
  bool result = ReadFile(input_file_path, buffer, info.size);

  *output = buffer;
  delete[] buffer;
  return result;
}

bool ConvertHexStringToInt(const std::string& hex_string, int* output_value) {
  if (hex_string.empty())
    return false;
  if (hex_string.find_first_not_of(kHexNumberTokens) != hex_string.npos)
    return false;
  std::stringstream string_stream;
  string_stream << std::hex << hex_string;
  unsigned int hex_number;
  string_stream >> hex_number;
  if (output_value)
    *output_value = hex_number;
  return true;
}

bool VerifyDeviceResponse(const std::vector<char>& hid_return_msg,
                          const std::string& isp_progress_word,
                          uint32_t start_position) {
  if (hid_return_msg.size() < isp_progress_word.size() + start_position) {
    LOG(ERROR) << "The hid_return_msg size is too small:"
               << hid_return_msg.size();
    return false;
  }

  if (std::equal(isp_progress_word.begin(), isp_progress_word.end(),
                 hid_return_msg.begin() + start_position))
    return true;
  else
    return false;
}

bool ExtractTarFile(const base::FilePath& temp_path,
                    const std::string& file_path) {
  struct archive* a;
  struct archive_entry* entry;
  int r;

  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  r = archive_read_open_filename(a, file_path.c_str(), 10240);
  if (r != ARCHIVE_OK)
    return false;

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    const char* fw_name;
    std::string command;

    /* no output file */
    if (archive_entry_pathname(entry) == NULL)
      return false;

    /* update output path */
    fw_name = archive_entry_pathname(entry);
    base::FilePath tmp_path = temp_path.Append(fw_name);
    bool vaild =
      archive_entry_update_pathname_utf8(entry, tmp_path.value().c_str());
    if (!vaild)
      continue;

    r = archive_read_extract(a, entry, 0);
    if (r != ARCHIVE_OK) {
      LOG(ERROR) << "Failed to read extract.";
      return false;
    }
  }
  r = archive_read_free(a);

  if (r != ARCHIVE_OK)
    return false;

  return true;
}
