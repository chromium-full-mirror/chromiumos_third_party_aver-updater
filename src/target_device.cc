// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "target_device.h"

#include <iostream>
#include <memory>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/logging.h>

#include "utilities.h"
#include "model_one_device.h"
#include "model_two_device.h"

namespace {
const char kVideoImagePath[] = "/lib/firmware/aver/";
const char kDefaultHidDeviceMountPoint[] = "../../sys/class/hidraw/";
const char kDevPath[] = "/dev/";
const char kVendorIdPath[] = "/device/../../idVendor";
const char kProductIdPath[] = "/device/../../idProduct";
const char kAverCAM520Name[] = "CAM520";
const char kAverCAM540Name[] = "CAM540";
const char kCAM520FwMidName[] = "";
const char kNewCAM540FwMidName[] = "videoh.dat";

constexpr unsigned int kDeviceVideo = 0;
constexpr unsigned int kAVerVendorID = 0x2574;
constexpr unsigned int kAVerCAM520ProductID = 0x0910;
constexpr unsigned int kAVerCAM340PlusProductID = 0x0980;
constexpr unsigned int kAVerCAM540ProductID = 0x0970;
constexpr unsigned int kAVerNewCAM540ProductID = 0x09A0;

}  // namespace

TargetDevice::TargetDevice()
    : is_untar_file_necessary_(true) {}

TargetDevice::~TargetDevice() {}

bool TargetDevice::AddDevice() {
  for (auto it = product_info_.begin(); it != product_info_.end(); it++) {
    int hid_type;
    std::string device_name;
    std::string fw_mid_name;

    switch (it->first) {
      case kAVerCAM520ProductID:
        hid_type = kModelOneDevice;
        device_name = kAverCAM520Name;
        fw_mid_name = kCAM520FwMidName;
        is_untar_file_necessary_ = false;
        break;
      case kAVerNewCAM540ProductID:
        hid_type = kModelOneDevice;
        device_name = kAverCAM540Name;
        fw_mid_name = kNewCAM540FwMidName;
        break;
      case kAVerCAM340PlusProductID:
      case kAVerCAM540ProductID:
        hid_type = kModelTwoDevice;
        break;
      default:
        hid_type = kModelUndefine;
        break;
    }

    switch (hid_type) {
      case kModelOneDevice:
        devices_.push_back(std::make_unique<ModelOneDevice>(
            device_name, it->second, fw_mid_name));
        break;
      case kModelTwoDevice:
        devices_.push_back(std::make_unique<ModelTwoDevice>(it->second));
        break;
      default:
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
  AverStatus status = AverStatus::NO_ERROR;
  if (is_untar_file_necessary_) {
    status = UntarFirmware();
    if (status != AverStatus::NO_ERROR)
      return status;
  }

  for (const auto& device : devices_) {
    status = device->PerformUpdate(temp_path_);
    if (status != AverStatus::NO_ERROR)
      break;
  }

  if (is_untar_file_necessary_) {
    if (!base::DeletePathRecursively(temp_path_))
        LOG(ERROR) << "Failed to delete untar firmware.";
  }

  return status;
}

bool TargetDevice::FindHidDevicesByPid(uint64_t which_device) {
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

    if (vidnum == kAVerVendorID) {
      // Target device may be a composite device with video and audio
      if (pidnum == which_device || pidnum == (which_device + 1))
        product_info_[pidnum] = kDevPath + content;
    }
  }

  if (product_info_.empty())
    return false;

  return true;
}

AverStatus TargetDevice::UntarFirmware() {
  std::string file_path = kVideoImagePath;
  std::string firmware_version;
  GetImageVersion(&firmware_version);
  file_path.append(firmware_version);

  if (!base::CreateNewTempDirectory("aver_updater_tmp_dir", &temp_path_)) {
    LOG(ERROR) << "Failed to create temp directory.";
    return AverStatus::FAILED_CREATE_TMP_PATH;
  }

  bool ok = false;
  ok = ExtractTarFile(temp_path_, file_path);
  if (!ok)
    return AverStatus::FAILED_EXTRACT_COMPRESSED_FW;

  return AverStatus::NO_ERROR;
}
