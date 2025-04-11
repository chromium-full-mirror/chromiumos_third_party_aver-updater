// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "utilities.h"
#include "usb_device.h"
#include "model_one_device.h"
#include "model_two_device.h"
#include "target_device.h"

namespace {
/**
 * @brief Configures the logging system.
 * @param log_file Specifies the log file to redirect to or stdout for console
 * output.
 */
void ConfigureLogging(std::string log_file) {
  if (log_file.empty()) {
    brillo::InitLog(brillo::InitFlags::kLogToSyslog |
                    brillo::InitFlags::kLogToStderrIfTty);
  } else if (log_file == "stdout") {
    logging::LoggingSettings logging_settings;
    logging::InitLogging(logging_settings);
  } else {
    logging::LoggingSettings logging_settings;
    logging_settings.logging_dest = logging::LOG_TO_FILE;
    logging_settings.log_file_path = log_file.c_str();
    logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
    logging::InitLogging(logging_settings);
  }
}

void LogStatus(AverStatus status) {
  switch (status) {
    case AverStatus::USB_HID_NOT_FOUND:
      LOG(ERROR) << "Failed to open the hid device.";
      break;
    case AverStatus::USB_UVC_NOT_FOUND:
      LOG(ERROR) << "Failed to open the uvc device.";
      break;
    case AverStatus::OPEN_FOLDER_PATH_FAILED:
      LOG(ERROR) << "Failed to open file folder path.";
      break;
    case AverStatus::DEVICE_NOT_OPEN:
      LOG(ERROR) << "Failed to open Video Device.";
      break;
    case AverStatus::IO_CONTROL_OPERATION_FAILED:
      LOG(ERROR) << "Failed to do ioctl.";
      break;
    case AverStatus::DIR_CONTENT_ERR:
      LOG(ERROR) << "Failed to get directory content.";
      break;
    case AverStatus::FW_ALREADY_UPDATE:
      LOG(INFO) << "Firmware is up to date.";
      break;
    case AverStatus::READ_FW_TO_BUF_FAILED:
      LOG(ERROR) << "Failed to read fimrware data.";
      break;
    case AverStatus::READ_COMPRESSED_FW_FAILED:
      LOG(ERROR) << "Failed to read the compressed firmware.";
      break;
    case AverStatus::FAILED_CREATE_TMP_PATH:
      LOG(ERROR) << "Failed to create the temp path.";
      break;
    case AverStatus::FAILED_EXTRACT_COMPRESSED_FW:
      LOG(ERROR) << "Failed to extract the compressed firmware.";
      break;
    case AverStatus::FAILED_DELETE_FW:
      LOG(ERROR) << "Failed to delete untar firmware.";
      break;
    default:
      LOG(ERROR) << "Unexpected status.";
      break;
  }
}

template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&& ... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}
}  // namespace

/**
 * @brief main function.
 * We restruct and modify our source codes :
 * 1. Delete the source codes about VB342 FW update flow.
 * 2. In order not to confuse functionality, change the category name :
 *    VideoDevice  ->  UsbDevice,
 *    CAM520Camera ->  ModelOneDevice,
 *    FourKDevice  ->  ModelTwoDevice.
 * 3. Add TargetDevice for present products & furture composite device product.
 * 4. Remove product names from ModelOneDevice and ModelTwoDevice.
 * 5. Simplify firmware update flow of two models.
 * 6. Product ID from udev rule to verify which product hotplug-in.
 * 7. Move uvc command from CAM520Camera to UsbDevice.
 */
int main(int argc, char** argv) {
  DEFINE_bool(update, false, "Perform firmware update.");
  DEFINE_bool(force, false, "Force firmware update.");
  DEFINE_bool(device_version, false, "Show firmware version in the device.");
  DEFINE_bool(image_version, false, "Show firmware version in Chromebox stored position.");
  DEFINE_bool(lock, true, "Make sure only 1 updater can be run during update");
  DEFINE_uint64(which_device, 0, "Which aver product plug-in");
  DEFINE_string(log_to, "", "Specify log file to write messages to.");
  brillo::FlagHelper::Init(argc, argv, "aver-updater");

  ConfigureLogging(FLAGS_log_to);

  if (FLAGS_lock && !LockUpdater()) {
    LOG(ERROR) << "There is another aver-updater running. Exiting now...";
    return 0;
  }

  AverStatus status = AverStatus::NO_ERROR;

  std::unique_ptr<TargetDevice> aver_device = make_unique<TargetDevice>();

  if (!aver_device->FindHidDevicesByPid(FLAGS_which_device)) {
    LOG(ERROR) << "Failed to find product id.";
    return 0;
  }

  if (!aver_device->AddDevice()) {
    LOG(ERROR) << "No matching product.";
    return 0;
  }

  status = aver_device->OpenDevice();
  if (status != AverStatus::NO_ERROR) {
    LogStatus(status);
    return 1;
  }

  if (FLAGS_device_version) {
    std::string device_version;
    status = aver_device->GetDeviceVersion(&device_version);
    if (status != AverStatus::NO_ERROR) {
      LogStatus(status);
      return 1;
    }
    LOG(INFO) << "Firmware version on the device:" << device_version;
  }

  if (FLAGS_image_version) {
    std::string image_version;
    status = aver_device->GetImageVersion(&image_version);
    if (status != AverStatus::NO_ERROR) {
      LogStatus(status);
      return 1;
    }
    LOG(INFO) << "Latest firmware available:" << image_version;
  }

  if (FLAGS_update) {
    status = aver_device->IsDeviceUpToDate(FLAGS_force);
    if (status != AverStatus::NO_ERROR) {
      LogStatus(status);
      return 1;
    }

    LOG(INFO) << "Updating device.";
    status = aver_device->PerformUpdate();
    if (status != AverStatus::NO_ERROR) {
      LogStatus(status);
      return 1;
    }
    LOG(INFO) << "Done. Updated firmware successfully.";
  }
  return 0;
}
