// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_VIDEO_DEVICE_H_
#define SRC_VIDEO_DEVICE_H_

#include <stdint.h>
#include <string>
#include <vector>

/**
 * @brief AVer extension unit command.
 * ISP means In-system programming.
 */
enum AverIspStatus {
    UVCX_UCAM_ISP_STATUS = 0x01, // Get isp status from AVer device .
    UVCX_UCAM_ISP_FILE_START,    // Notify AVer device to start isp download follow.
    UVCX_UCAM_ISP_FILE_DNLOAD,   // Notify AVer device to ready to receive isp file data.
    UVCX_UCAM_ISP_FILE_END,      // Notify AVer device to end up file download.
    UVCX_UCAM_ISP_START,         // Notify AVer device to start isp follow.
};

/**
 * @brief AVer error report.
 */
enum class AverStatus {
  NO_ERROR = 0,
  USB_PID_NOT_FOUND,
  USB_HID_NOT_FOUND,
  DEVICE_NOT_OPEN,
  OPEN_DEVICE_FAILED,
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
  READ_FW_TO_BUF_FAILED,
  UNKNOWN
};

/**
 * @brief report (output) (from pc)
 */
struct OutReport
{
  uint8_t id;                         /**< report id */
  uint8_t report_cmd;                 /**< report cmd */
  uint8_t isp_cmd;                    /**< isp cmd */
  uint8_t dat[508];                   /**< report data */
};

/**
 * AVer video device class to handle video firmware update.
 */
class VideoDevice final {
 public:

  /**
   * @brief Constructor with product id.
   * @param pid Product id string.
   */
  VideoDevice(const std::string& pid);

  ~VideoDevice();

  /**
   * @brief Opens the device.
   * @return NO_ERROR if opened ok, error code otherwise.
   */
  AverStatus OpenDevice();

  /**
   * @brief Closes the device.
   */
  void CloseDevice();

  /**
   * @brief Performs firmware update.
   * @return NO_ERROR if updated ok, error code otherwise.
   */
  AverStatus PerformUpdate();

  /**
   * @brief Gets the device version.
   * @param device_version Output device version string.
   * @return NO_ERROR if failed, error code otherwise.
   */
  AverStatus GetDeviceVersion(std::string* device_version);

  /**
   * @brief Checks if device firmware is up to date.
   * @param force Force the device to do firmware update.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IsDeviceUpToDate(bool force);

  /**
   * @brief Load firmware to the buffer.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus LoadFirmwareToBuffer();

  /**
   * @brief Gets the image version.
   * @param image_version_ Output device version string.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus GetImageVersion(std::string* image_version_);

  /**
   * @brief Reads the device version.
   * @param device_version_ Output device version string.
   * @return NO_ERROR if read ok, error code otherwise.
   */
  AverStatus ReadDeviceVersion(std::string* device_version_);

 private:

  /**
   * @brief Finds the device with usb_pid & usb_vid.
   * @return device path to open the video device.
   */
  std::string FindDevice();

  /**
   * @brief Finds the hid device with usb_pid & usb_vid.
   * @return device path to open the hid device.
   */
  std::string FindHidDevice();

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


  /**
   * @brief Check device isp status.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspStatusGet();

  /**
   * @brief Send isp file name to CAM520.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspFileStart();

  /**
   * @brief Download isp file to CAM520.
   * @param buffer Buffer full with firmware data.
   * @param isp_file_size Firmware data size.
   * @return NO_ERROR if succeeded, error code otherwise.
   */
  AverStatus IspFileDownload(std::vector<char> buffer, uint32_t isp_file_size);

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
   * @brief Gets data from the extension control unit (XU).
   * @param unit_id XU unit id.
   * @param control_selector XU control selector.
   * @param data XU data output.
   * @return NO_ERROR if succeeded, error otherwise.
   */
  AverStatus GetXuControl(unsigned char unit_id,
                   unsigned char control_selector,
                   std::string* data);

  /**
   * @brief Sends data to the hid control unit.
   * @param customize_cmd AVer hid command structure.
   * @param data HID data output.
   * @return NO_ERROR if succeeded, error otherwise.
   */
  AverStatus SendHidControl(const struct OutReport &customize_cmd);

  std::string usb_pid_; // The string is USB product id.
  int file_descriptor_; // File descriptor is for video device.
  int file_descriptor_hid_; // Hid file descriptor is for hid device.
  bool is_open_; // The device already opened.
  std::string image_version_;  // Image version is firmware version from google cloud position.
  std::string device_version_;  // Device version is firmware from the device.
  std::vector<char> hid_return_msg_; // Messages return from hid device.

  /**
   * @brief Buffer puts isp firmware.
   */
  std::vector<char> firmware_buffer_;

};
#endif  // SRC_VIDEO_DEVICE_H_