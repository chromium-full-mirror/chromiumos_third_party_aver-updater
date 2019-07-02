// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "target_device.h"

#include <iostream>
#include <memory>

#include "utilities.h"
#include "model_one_device.h"
#include "model_two_device.h"

namespace {
const char kDefaultHidDeviceMountPoint[] = "../../sys/class/hidraw/";
const char kDevPath[] = "/dev/";
const char kVendorIdPath[] = "/device/../../idVendor";
const char kProductIdPath[] = "/device/../../idProduct";
const char kAverCAM520Name[] = "CAM520";

constexpr unsigned int kDeviceVideo = 0;
constexpr unsigned int kAVerVendorID = 0x2574;
constexpr unsigned int kAVerCAM520ProductID = 0x0910;
constexpr unsigned int kAVerCAM340PlusProductID = 0x0980;
constexpr unsigned int kAVerCAM540ProductID = 0x0970;

template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&& ... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}
}  // namespace

TargetDevice::TargetDevice() {}

TargetDevice::~TargetDevice() {}

bool TargetDevice::AddDevice(uint64_t which_device) {
  int hid_type;
  std::string device_name;
  for (auto it = product_info_.begin(); it != product_info_.end(); it++) {
    // Target device may be a composite device with video and audio
    // in the future. Product id will diff by one.
    if (it->first != which_device && (it->first - 1) != which_device)
      continue;

    switch (it->first) {
      case kAVerCAM520ProductID:
        hid_type = kModelOneDevice;
        device_name = kAverCAM520Name;
        break;
      case kAVerCAM340PlusProductID:
      case kAVerCAM540ProductID:
        hid_type = kModelTwoDevice;
        break;
    }

    switch (hid_type) {
        case kModelOneDevice:
          devices_.push_back(make_unique<ModelOneDevice>(device_name,
                                                         it->second));
          break;
        case kModelTwoDevice:
          devices_.push_back(make_unique<ModelTwoDevice>(it->second));
          break;
    }
  }

  if (devices_.empty())
    return false;
  return true;
}

AverStatus TargetDevice::OpenDevice() {
  AverStatus status;
  for (const auto& device : devices_) {
    status = device->OpenDevice();
    if (status != AverStatus::NO_ERROR)
      return status;
  }
  return status;
}

AverStatus TargetDevice::GetDeviceVersion(std::string* device_version) {
  AverStatus status;
  status = devices_[kDeviceVideo]->GetDeviceVersion(device_version);
  return status;
}

AverStatus TargetDevice::GetImageVersion(std::string* image_version) {
  AverStatus status;
  std::string device_version;
  status = devices_[kDeviceVideo]->GetDeviceVersion(&device_version);
  if (status != AverStatus::NO_ERROR)
    return status;
  status = devices_[kDeviceVideo]->GetImageVersion(device_version,
                                                   image_version);
  return status;
}

AverStatus TargetDevice::IsDeviceUpToDate(bool force) {
  AverStatus status = devices_[kDeviceVideo]->IsDeviceUpToDate(force);
  return status;
}

AverStatus TargetDevice::PerformUpdate() {
  AverStatus status;
  for (const auto& device : devices_) {
    status = device->PerformUpdate();
    if (status != AverStatus::NO_ERROR)
      return status;
  }
  return status;
}

bool TargetDevice::FindHidDevice() {
  std::vector<std::string> contents;
  bool get_ok = GetDirectoryContents(kDefaultHidDeviceMountPoint, &contents);
  if (!get_ok)
    return false;

  base::FilePath hid_dir(kDefaultHidDeviceMountPoint);
  for (auto const& content : contents) {
    if (content.compare(".") == 0 || content.compare("..") == 0)
      continue;

    std::string vid;
    std::string pid;

    base::FilePath vid_path = hid_dir.Append(content + kVendorIdPath);
    base::FilePath pid_path = hid_dir.Append(content + kProductIdPath);
    if (!ReadFileContent(vid_path, &vid))
      continue;

    int vidnum = 0;
    int pidnum = 0;
    if (!ConvertHexStringToInt(vid, &vidnum))
      continue;

    if (!ReadFileContent(pid_path, &pid))
      continue;

    if (!ConvertHexStringToInt(pid, &pidnum))
      continue;

    if (vidnum == kAVerVendorID)
      product_info_[pidnum] = kDevPath + content;
  }

  if (product_info_.empty())
    return false;

  return true;
}