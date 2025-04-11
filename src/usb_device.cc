// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_device.h"

#include <fcntl.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <sys/ioctl.h>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>

#include "utilities.h"

namespace {
const char kVideoImagePath[] = "/lib/firmware/aver/";
const char kDefaultVideoDeviceMountPoint[] = "/sys/class/video4linux/";
const char kName[] = "/name";

constexpr unsigned int kAVerFirmwareCompareCount = 6;
constexpr unsigned int kAVerFirmwareSuffixLength = 15;
constexpr unsigned int kAVerFirmwareExtensionLength = 3;
constexpr unsigned int kDefaultUvcGetLenQueryControlSize = 2;
}  // namespace

UsbDevice::UsbDevice() {}

UsbDevice::~UsbDevice() {
  if (uvc_file_descriptor_.is_valid())
    uvc_file_descriptor_.reset();
}

base::ScopedFD UsbDevice::CreateFd(const std::string& device_path) {
  base::ScopedFD fd(HANDLE_EINTR(open(device_path.c_str(), O_RDWR)));
  return fd;
}

AverStatus UsbDevice::IsDeviceUpToDate(bool force) {
  AverStatus status;
  status = GetDeviceVersion(&device_version_);
  if (status != AverStatus::NO_ERROR)
    return status;

  LOG(INFO) << "Firmware version on the device:" << device_version_;

  if (force)
    return AverStatus::NO_ERROR;

  status = GetImageVersion(device_version_, &firmware_version_);
  if (status != AverStatus::NO_ERROR)
    return status;

  LOG(INFO) << "Latest firmware available:" << firmware_version_;

  std::string img_ver = firmware_version_.substr(firmware_version_.length() -
                                                 kAVerFirmwareSuffixLength);

  if (CompareVersions(device_version_, img_ver) < 0)
    return AverStatus::NO_ERROR;

  return AverStatus::FW_ALREADY_UPDATE;
}

AverStatus UsbDevice::GetImageVersion(const std::string& device_version,
                                      std::string* image_version) {
  std::vector<std::string> files;
  bool get_ok = GetDirectoryContents(kVideoImagePath, &files);
  if (!get_ok)
    return AverStatus::DIR_CONTENT_ERR;

  // Get device firmware version to compare.
  std::string firmware_version_prefix;
  firmware_version_prefix.assign(device_version.begin(),
                                 device_version.begin() +
                                 kAVerFirmwareCompareCount);

  auto selected_file_it = files.end();
  for (auto it = files.begin(); it != files.end(); it++) {
    if (it->substr(0, 1) == "." || it->substr(0, 2) == "..")
      continue;

    std::string img_ver = it->substr(it->length() - kAVerFirmwareSuffixLength);
    if (img_ver.substr(0, kAVerFirmwareCompareCount) != firmware_version_prefix)
        continue;

    if (selected_file_it == files.end() || *selected_file_it < *it) {
      selected_file_it = it;
    }
  }

  if (selected_file_it == files.end())
    return AverStatus::DIR_CONTENT_ERR;

  *image_version = firmware_version_ = *selected_file_it;
  return AverStatus::NO_ERROR;
}

bool UsbDevice::OpenUvcDevice(const std::string& device_name) {
  std::string dev_path;
  std::vector<std::string> contents;
  bool get_ok = GetDirectoryContents(kDefaultVideoDeviceMountPoint, &contents);
  if (!get_ok)
    return false;

  std::string video_dir(kDefaultVideoDeviceMountPoint);
  for (auto const& content : contents) {
    if (content.compare(".") == 0 || content.compare("..") == 0)
      continue;

    std::string name_path = video_dir + content + kName;
    std::string name;

    base::FilePath input_file_path(name_path.c_str());
    base::ReadFileToString(input_file_path, &name);

    if (name.find(device_name) != name.npos)
      dev_path = "/dev/" + content;
  }

  uvc_file_descriptor_ = CreateFd(dev_path.c_str());
  if (!uvc_file_descriptor_.is_valid()) {
    PLOG(ERROR) << "Open uvc fd fail";
    return false;
  }

  return true;
}

AverStatus UsbDevice::GetXuControl(unsigned char unit_id,
                                   unsigned char control_selector,
                                   std::string* data) {
  int data_len;
  AverStatus status = QueryDataSize(unit_id, control_selector, &data_len);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Query data size failed.";
    return status;
  }

  std::vector<uint8_t> query_data(data_len);

  struct uvc_xu_control_query control_query;
  control_query.unit = unit_id;
  control_query.selector = control_selector;
  control_query.query = UVC_GET_CUR;
  control_query.size = data_len;
  control_query.data = &query_data[0];
  int err = HANDLE_EINTR(ioctl(uvc_file_descriptor_.get(),
                               UVCIOC_CTRL_QUERY,
                               &control_query));
  if (err < 0) {
    LOG(ERROR) << "Uvc ioctl query failed.";
    return AverStatus::IO_CONTROL_OPERATION_FAILED;
  }

  for (int i = 0; i < data_len; i++)
    data->push_back(query_data[i]);

  return AverStatus::NO_ERROR;
}

AverStatus UsbDevice::QueryDataSize(unsigned char unit_id,
                                    unsigned char control_selector,
                                    int* data_size) {
  uint8_t size_data[kDefaultUvcGetLenQueryControlSize];
  struct uvc_xu_control_query size_query;
  size_query.unit = unit_id;
  size_query.selector = control_selector;
  size_query.query = UVC_GET_LEN;
  size_query.size = kDefaultUvcGetLenQueryControlSize;
  size_query.data = size_data;
  int status = HANDLE_EINTR(ioctl(uvc_file_descriptor_.get(),
                                  UVCIOC_CTRL_QUERY,
                                  &size_query));
  if (status < 0)
    return AverStatus::IO_CONTROL_OPERATION_FAILED;

  int size = (size_data[1] << 8) | (size_data[0]);
  *data_size = size;
  return AverStatus::NO_ERROR;
}

std::string UsbDevice::GetVersion(const std::string& fw_name) {
  return fw_name.substr(fw_name.length() - kAVerFirmwareSuffixLength,
           kAVerFirmwareSuffixLength - kAVerFirmwareExtensionLength);
}
