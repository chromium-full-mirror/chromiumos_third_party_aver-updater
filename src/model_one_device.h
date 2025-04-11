// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MODEL_ONE_DEVICE_H_
#define SRC_MODEL_ONE_DEVICE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include <base/files/file.h>

#include "usb_device.h"

/**
 * @brief AVer extension unit command.
 * ISP means In-system programming.
 */
enum AverIspStatus {
  UVCX_UCAM_ISP_STATUS = 0x01,  // Get isp status from AVer device .
  UVCX_UCAM_ISP_FILE_START,  // Notify AVer device to start isp download follow.
  UVCX_UCAM_ISP_FILE_DNLOAD,    // Notify AVer device to ready to
  // receive isp file data.
  UVCX_UCAM_ISP_FILE_END,       // Notify AVer device to end up file download.
  UVCX_UCAM_ISP_START,          // Notify AVer device to start isp follow.
  UVCX_UCAM_FW_VERSION = 0x14,  // Check device frimware version.
};

/**
 * @brief Model 1 hid version one report (input) (to pc)
 */
struct ModelOneDeviceInReport {
  uint8_t id;                         /**< report id */
  uint8_t dat[512 - sizeof(uint8_t)]; /**< report data */
};

/**
 * @brief Model 1 hid version one report (output) (from pc)
 */
struct ModelOneDeviceOutReport {
  uint8_t id;         /**< report id */
  uint8_t report_cmd; /**< report cmd */
  uint8_t isp_cmd;    /**< isp cmd */
  uint8_t dat[508];   /**< report data */
};

/**
 * Model 1 class to handle video firmware update.
 */
class ModelOneDevice : public UsbDevice {
 public:
  /**
   * @brief Constructor.
   */
  explicit ModelOneDevice(const std::string& device_name,
                          const std::string& dev_path,
                          const std::string& fw_mid_name);
  ~ModelOneDevice() override;

  /**
   * @brief Opens device.
   * @return NO_ERROR if opened ok, error code otherwise.
   */
  AverStatus OpenDevice() override;

  /**
   * @brief Performs firmware update.
   * @param tmp_path Temporary folder path.
   * @return NO_ERROR if updated ok, error code otherwise.
   */
  AverStatus PerformUpdate(const base::FilePath& tmp_path) override;

  /**
   * @brief Gets the device version.
   * @param device_version Output device version string.
   * @return NO_ERROR if failed, error code otherwise.
   */
  AverStatus GetDeviceVersion(std::string* device_version) override;

  /**
   * @brief Load firmware to the buffer.
   * @param image_version Firmware version.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus LoadFirmwareToBuffer() override;

 private:
  /**
   * @brief Check device isp status.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspStatusGet();

  /**
   * @brief Send isp file name.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspFileStart();

  /**
   * @brief Download isp file.
   * @param buffer Buffer full with firmware data.
   * @param isp_file_size Firmware data size.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspFileDownload(const std::vector<char>& buffer,
                             uint32_t isp_file_size);

  /**
   * @brief Send the commend that downdloading isp file is finish.
   * @param isp_file_size Firmware data size.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspFileEnd(uint32_t isp_file_size);

  /**
   * @brief Send the commend to start isp follow.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspStart();

  /**
   * @brief Sends data to the hid control unit.
   * @param customize_cmd AVer hid command structure.
   * @return NO_ERROR if succeeded, error otherwise.
   */
  AverStatus SendHidVerOneControl(const struct ModelOneDeviceOutReport&
                                  customize_cmd);

  /**
   * @brief Device firmware update progress.
   * @return NO_ERROR if succeeded, error otherwise.
   */
  AverStatus FirmwareUpdate();

  // Device name.
  std::string device_name_;
  // Hid firmware device path.
  std::string device_path_;
  // Firmware middle name.
  std::string firmware_middle_name_;
  // Messages return from hid device.
  std::vector<char> hid_return_msg_;
  // File descriptor is hidraw device fd.
  base::ScopedFD file_descriptor_hid_;
  // Buffer puts isp firmware.
  std::vector<char> firmware_buffer_;
  // Firmware temp path.
  base::FilePath temp_path_;
};

#endif  // SRC_MODEL_ONE_DEVICE_H_
