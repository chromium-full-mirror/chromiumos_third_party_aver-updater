// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_TARGET_DEVICE_H_
#define SRC_TARGET_DEVICE_H_

#include <map>
#include <vector>

#include "usb_device.h"

/**
 * Target device class is composite device include video, audio device and etc.
 */
class TargetDevice {
 public:
  /**
   * @brief Constructor.
   */
  TargetDevice();
  ~TargetDevice();

  /**
   * @brief Add device to this class.
   * @return true for find product id, false for failed.
   */
  bool AddDevice();

  /**
   * @brief Opens device.
   * @return NO_ERROR if opened ok, error code otherwise.
   */
  AverStatus OpenDevice();

  /**
   * @brief Gets the device version.
   * @param device_version Output device version string.
   * @return NO_ERROR if failed, error code otherwise.
   */
  AverStatus GetDeviceVersion(std::string* device_version);

  /**
   * @brief Gets the image version.
   * @param image_version Output device version string.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus GetImageVersion(std::string* image_version);

  /**
   * @brief Checks if device firmware is up to date.
   * @param force Force the device to do firmware update.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IsDeviceUpToDate(bool force);

  /**
   * @brief Performs firmware update.
   * @return NO_ERROR if updated ok, error code otherwise.
   */
  AverStatus PerformUpdate();

  /**
   * @brief Finds the hid device product and device path.
   * @param which_device Which aver device plug-in.
   * @return true for find product id, false for failed.
   */
  bool FindHidDevicesByPid(uint64_t which_device);

  /**
   * @brief Untar firmware.
   * @return NO_ERROR if updated ok, error code otherwise.
   */
  AverStatus UntarFirmware();

 private:
  // Product id & hid device path
  std::map<int, std::string> product_info_;
  // Devices for video, audio and etc..
  std::vector<std::unique_ptr<UsbDevice>> devices_;
  // Firmware temp path.
  base::FilePath temp_path_;
  // need untar firmware file or not
  bool is_untar_file_necessary_;
};
#endif  // SRC_TARGET_DEVICE_H_
