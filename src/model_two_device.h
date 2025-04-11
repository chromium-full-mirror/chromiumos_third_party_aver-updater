// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MODEL_TWO_DEVICE_H_
#define SRC_MODEL_TWO_DEVICE_H_

#include <map>

#include "usb_device.h"

/**
 * @brief AVer custom isp command.
 * ISP means In-system programming.
 */
enum Aver4KIspStatus {
  HID_CSTM_CMD_NONE = 0,
  HID_CSTM_CMD_CX3_FLASH_PREPARE,
  HID_CSTM_CMD_CX3_FLASH_UPLOAD,
  HID_CSTM_CMD_CX3_FLASH_DONE,
  HID_CSTM_CMD_M12MO_FLASH_PREPARE,
  HID_CSTM_CMD_M12MO_FLASH_UPLOAD,
  HID_CSTM_CMD_M12MO_FLASH_DONE,
  HID_CSTM_CMD_CX3_REBOOT,
  HID_CSTM_CMD_FACTORY_GET_FW_VERSION = 20,
  HID_CSTM_CMD_M051_FLASH_PREPARE = 31,
  HID_CSTM_CMD_M051_FLASH_UPLOAD = 32,
  HID_CSTM_CMD_M051_FLASH_VERIFY = 33,
  HID_CSTM_CMD_M051_FLASH_DONE = 34,
  HID_CSTM_CMD_FLASH_COMPARE_CHECKSUM = 35,
  HID_CSTM_CMD_TMPM342_BOOT = 36,
  HID_CSTM_CMD_TMPM342_SEND = 37,
  HID_CSTM_CMD_TMPM342_RECV = 38,

  HID_CSTM_CMD_SAFE_ISP_SUPPORT = 41,
  HID_CSTM_CMD_SAFE_ISP_ERASE_TEMP = 42,
  HID_CSTM_CMD_SAFE_ISP_UPLOAD_PREPARE = 43,
  HID_CSTM_CMD_SAFE_ISP_UPLOAD_COMPARE_CHECKSUM = 44,
  HID_CSTM_CMD_SAFE_ISP_UPLOAD_TO_CX3 = 45,
  HID_CSTM_CMD_SAFE_ISP_UPLOAD_TO_M12MO = 46,
  HID_CSTM_CMD_SAFE_ISP_UPLOAD_TO_M051 = 47,
  HID_CSTM_CMD_SAFE_ISP_UPLOAD_TO_TMPM342 = 48,
  HID_CSTM_CMD_SAFE_ISP_UPLOAD_TO_TMPM342_BOOT = 49,
  HID_CSTM_CMD_SAFE_ISP_UPDATA_START = 50,
  HID_CSTM_CMD_M12MO_COMPARE_CHECKSUM = 51,
};

/**
 * @brief AVer hid acknowledge status.
 */
enum HidAckStatus {
  HID_ACK_IDLE = 0,
  HID_ACK_SUCCESS,
  HID_ACK_CHECKSUM,
  HID_ACK_PARAM_ERR,
  HID_ACK_PREPARE_FAIL,
  HID_ACK_UPLOAD_FAIL,
  HID_ACK_DAT_RD,
  HID_ACK_DAT_RD_FAIL,
  HID_ACK_DAT_WR,
  HID_ACK_DAT_WR_FAIL,
  HID_ACK_VERIFY_FAIL,
  HID_ACK_COMPARE_SAME,
  HID_ACK_COMPARE_DIFF,
  HID_ACK_SAFE_ISP_SUPPORT,
  HID_ACK_MAX,
};

/**
 * @brief Model 2 firmware upload function.
 */
enum ModelTwoUploadName {
  CX3_FW_UPLOAD = 0,
  M12MO_FW_UPLOAD,
  M051_FW_UPLOAD,
  HAWKEYE_FW_UPLOAD,
  BOOTIMG_FW_UPLOAD,
};

/**
 * @brief Model 2 hid video report (input) (to pc)
 */
struct ModelTwoDeviceInReport {
  uint8_t id;      /**< report id */
  uint8_t cmd;     /**< report command */
  uint8_t dat[14]; /**< report data */
};

/**
 * @brief Model 2 hid video report (output) (from pc)
 */
struct ModelTwoDeviceOutReport {
  uint8_t id;        /**< report id */
  uint8_t cmd;       /**< report command */
  uint16_t reserved; /**< reserved */
  uint32_t parm[2];  /**< report parameter */
  uint8_t dat[1016]; /**< report data */
};

/**
 * Mdoel 2 class to handle video firmware update.
 */
class ModelTwoDevice : public UsbDevice {
 public:
  explicit ModelTwoDevice(const std::string& device_path);
  ~ModelTwoDevice() override;
  /**
   * @brief Opens device.
   * @return NO_ERROR if opened ok, error code otherwise.
   */
  AverStatus OpenDevice() override;

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

  /**
   * @brief Performs firmware update.
   * @param tmp_path Temporary folder path.
   * @return NO_ERROR if updated ok, error code otherwise.
   */
  AverStatus PerformUpdate(const base::FilePath& tmp_path) override;

 private:
  /**
   * @brief Sends data to the hid control unit.
   * @param customize_cmd AVer hid command structure.
   * @return NO_ERROR if succeeded, error otherwise.
   */
  AverStatus SendHidVerTwoControl(const struct ModelTwoDeviceOutReport&
                                  customize_cmd);

  /**
   * @brief Reads the device version.
   * @return NO_ERROR if read ok, error code otherwise.
   */
  AverStatus ReadDeviceVersion();

  /**
   * @brief Handle Compressed firmware flow.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus HandleCompressedFirmware();

  /**
   * @brief New isp progress support.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspSupport();

  /**
   * @brief Prepare device to enter isp progress.
   * @param cmd Command for device isp.
   * @param chip_select Select the firmware of chip to upload.
   * @param sector_count SPI flash sector count.
   * @param file_sizr Nano100 firmware file size.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspPrepare(uint8_t cmd,
                        uint32_t chip_select,
                        uint32_t sector_count,
                        uint32_t file_size);

  /**
   * @brief Erase flash.
   * @param cmd Command for device isp.
   * @param chip_select Select the firmware of chip to upload.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspErase(uint8_t cmd, uint32_t chip_select);

  /**
   * @brief Upload isp file to aver product.
   * @parm cmd Command for device isp.
   * @param buffer Buffer full with firmware data.
   * @param isp_file_size Firmware data size.
   * @param chip_block_size Chip block size.
   * @param upload_select Select the specific firmware to upload.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspUpload(uint8_t cmd,
                       const std::vector<char>& buffer,
                       uint32_t isp_file_size,
                       uint32_t chip_block_size,
                       uint32_t upload_select);

  /**
   * @brief Update the device.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspUpdate();

  /**
   * @brief Send the commend to do checksum.
   * @param cmd Isp command.
   * @param i2c_adress I2c bus address in device.
   * @param chip_select Select the firmware of chip to upload.
   * @param buffer Buffer full with firmware data.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspCompareChecksum(uint8_t cmd,
                                uint32_t i2c_address,
                                uint32_t chip_select,
                                const std::vector<char>& buffer);

  /**
   * @brief Firmware Update.
   * @return NO_ERROR if succeeded, error otherwise.
   */
  AverStatus FirmwareUpdate();

  /**
   * @brief Firmware Upload to devices.
   * param index Index for device number.
   * @return NO_ERROR if succeeded, error otherwise.
   */
  AverStatus FirmwareUpload(int index);

  // Hid video device path.
  std::string device_path_;
  // File descriptor is hidraw device for video.
  base::ScopedFD file_descriptor_;
  // Device firmware update pass.
  bool device_update_[5];
  // Messages buffer return from hid device.
  struct ModelTwoDeviceInReport hid_info_;
  // Firmware temp path.
  base::FilePath temp_path_;
  // Firmware checksum.
  uint32_t checksum_;
  // Device firmware number include in compressed file
  int device_fw_num_;
  // Buffer puts isp firmware.
  std::map<int, std::vector<char>> firmware_buffer_;
};

#endif  // SRC_MODEL_TWO_DEVICE_H_
