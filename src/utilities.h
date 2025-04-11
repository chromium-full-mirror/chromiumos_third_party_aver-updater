// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_UTILITIES_H_
#define SRC_UTILITIES_H_

#include <stdio.h>
#include <string>
#include <vector>

#include <base/files/file_path.h>

/**
 * @brief Gets contents of the directory.
 * @param directory The directory path to get contents.
 * @param contents Ouput vector of the content paths.
 * @return true if succeeded, false otherwise.
 */
bool GetDirectoryContents(const std::string& directory,
                          std::vector<std::string>* contents);

/**
 * @brief Finds intermediate compressed firmware file.
 * The intermediate compressed firmware files are divided into video and audio parts.
 * @param folder Target folder we mant to search.
 * @param firmware Target firmware we want to find.
 * @return true for find the file, false for failed.
 */
bool FindIntermediateCompressedFile(const base::FilePath& folder,
                                    const std::string& firmware_name);

/**
 * @brief Reads binary file content to buffer.
 * @param filepath Path to the binary file.
 * @param buffer It's a buffer to fill firmware.
 * @return buffer from reading. Buffer is empty if read failed.
 */
bool ReadFirmwareFileToBuffer(const base::FilePath& file_path,
                              std::vector<char>* buffer);

/**
 * @brief Compares 2 version strings in format major.minor.build or major.minor.
 * @param version1 The version string 1.
 * @param version2 The version string 2.
 * @return 0 equal, -1 if version1 < version2 or 1 if version1 > version2.
 */
int CompareVersions(const std::string& version1, const std::string& version2);

/**
 * @brief Locks this updater to have only 1 instance running.
 * @return true if there is no other aver-updater processed running and was
 * able to lock the lock file, false otherwise.
 */
bool LockUpdater();

/**
 * @brief Reads file contents.
 * @param input_file_path The file path to read.
 * @param output Output string from reading.
 * @return true if read ok, false otherwise.
 */
bool ReadFileContent(const base::FilePath& input_file_path,
                     std::string* output);

/**
 * @brief Converts hex string to int.
 * @param hex_string The input hex string.
 * @param output_value The output int value.
 * @return false if convert failed, true otherwise.
 */
bool ConvertHexStringToInt(const std::string& hex_string, int* output_value);

/**
 * @brief Verify hid device response.
 * @param hid_return_msg The msg is from hid device.
 * @param isp_progress_word Expected ISP progress word.
 * @param start_position Command start position.
 * @return true if succeeded, false otherwise.
 */
bool VerifyDeviceResponse(const std::vector<char>& hid_return_msg,
                          const std::string& isp_progress_word,
                          uint32_t start_position);

/**
 * @brief Compressed firmware untar.
 * @param temp_path Temperary folder path.
 * @param file_path Compressed file path.
 * @return true if succeeded, error otherwise.
 */
bool ExtractTarFile(const base::FilePath& temp_path,
                    const std::string& file_path);
#endif  // SRC_UTILITIES_H_
