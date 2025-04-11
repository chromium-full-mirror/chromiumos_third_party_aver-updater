// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_USB_DEVICE_H_
#define SRC_USB_DEVICE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include <base/files/file.h>
#include <base/files/scoped_file.h>
#include <base/posix/eintr_wrapper.h>

/**
 * @brief AVer error report.
 */
enum class AverStatus {
  NO_ERROR = 0,
  USB_UVC_NOT_FOUND,
  USB_HID_NOT_FOUND,
  DEVICE_NOT_OPEN,
  OPEN_HID_DEVICE_FAILED,
  READ_DEVICE_FAILED,
  WRITE_DEVICE_FAILED,
  INVALID_DEVICE_VERSION_DATA_SIZE,
  XU_UNIT_ID_INVALID,
  IO_CONTROL_OPERATION_FAILED,
  OPEN_FOLDER_PATH_FAILED,
  DIR_CONTENT_ERR,
  FW_ALREADY_UPDATE,
  ISP_START_FAILED,
  ISP_FILE_START_HID_CMD_COMPARE_FAILED,
  ISP_FILE_END_HID_CMD_COMPARE_FAILED,
  ISP_START_HID_CMD_COMPARE_FAILED,
  READ_COMPRESSED_FW_FAILED,
  READ_FW_TO_BUF_FAILED,
  FAILED_EXTRACT_COMPRESSED_FW,
  CHECKSUM_SAME,
  M051_VERIFY_FAILED,
  FAILED_SUPPORT_NEW_FW_UPDATE,
  FAILED_CREATE_TMP_PATH,
  FAILED_DELETE_FW,
  UNKNOWN
};


/**
 * @brief AVer hid structure class.
 */
enum AverHidDevice {
  kModelOneDevice = 0,
  kModelTwoDevice,
  kModelUndefine
};

/**
 * AVer usb device class.
 */
class UsbDevice {
 public:
  /**
   * @brief Constructor.
   */
  UsbDevice();
  virtual ~UsbDevice();

  /**
   * @brief Opens device.
   * @return NO_ERROR if opened ok, error code otherwise.
   */
  virtual AverStatus OpenDevice() = 0;

  /**
   * @brief Performs firmware update.
   * @param tmp_path Temporary folder path.
   * @return NO_ERROR if updated ok, error code otherwise.
   */
  virtual AverStatus PerformUpdate(const base::FilePath& tmp_path) = 0;

  /**
   * @brief Gets the device version.
   * @param device_version Output device version string.
   * @return NO_ERROR if failed, error code otherwise.
   */
  virtual AverStatus GetDeviceVersion(std::string* device_version) = 0;

  /**
   * @brief Checks if device firmware is up to date.
   * @param force Force the device to do firmware update.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IsDeviceUpToDate(bool force);

  /**
   * @brief Load firmware to the buffer.
   * @param image_version Firmware version.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  virtual AverStatus LoadFirmwareToBuffer() = 0;

  /**
   * @brief Gets the image version.
   * @param device_version Device firmware version.
   * @param image_version Firmware version in system.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus GetImageVersion(const std::string& device_version,
                             std::string* image_version);

  /**
   * @brief Create file descriptor.
   * @return base::ScopedFD.
   */
  base::ScopedFD CreateFd(const std::string& device_path);

  /**
　 * @brief Re-combine firmware name.
   * @return Firmware version string.
　 */
  std::string GetVersion(const std::string& firmware_name);

 protected:
  /**
   * @brief Finds the uvc device.
   * @param device_name Device name.
   * @return true if found uvc fd, false otherwise.
   */
  bool OpenUvcDevice(const std::string& device_name);

  /**
    * @brief Gets data from the extension control unit (XU).
    * @param unit_id XU unit id.
    * @param control_selector XU control selector.
    * @param data XU data output.
    * @return NO_ERROR if succeeded, error otherwise.
    */
  AverStatus GetXuControl(unsigned char unit_id,
                          unsigned char control_selector,
                          std::string* data);

  // Device version is firmware from the device.
  std::string device_version_;
  // Device firmware version.
  std::string firmware_version_;

 private:
  /**
    * @brief Control query data size needs to be known before querying.
    * Therefore, UVC_GET_LEN needs to be queried for the size first.
    * UVC_GET_LEN query returns 2 bytes of data in little endian. Refers to
    * https://linuxtv.org/downloads/v4l-dvb-api/v4l-drviers/uvcvideo.html for
    * more info.
    * @param unitID XU unit id.
    * @param control_selector XU control selector.
    * @param data_size Data size output.
    * @return NO_ERROR if succeeded, error code otherwise.
    */
  AverStatus QueryDataSize(unsigned char unit_id,
                           unsigned char control_selector,
                           int* data_size);

  // File descriptor is uvc device fd.
  base::ScopedFD uvc_file_descriptor_;
};

#endif  // SRC_USB_DEVICE_H_
