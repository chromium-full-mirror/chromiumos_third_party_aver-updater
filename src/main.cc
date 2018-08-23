// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include "video_device.h"
#include "utilities.h"

namespace {
const char kAVerCAM520Pid[] = "0x0910";     // It's the CAM520 product id.
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
    logging_settings.log_file = log_file.c_str();
    logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
    logging::InitLogging(logging_settings);
  }
}
}

int main(int argc, char** argv) {
  DEFINE_bool(update, false, "Perform firmware update.");
  DEFINE_bool(force, false, "Force firmware update.");
  DEFINE_bool(device_version, false, "Show firmware version in the device.");
  DEFINE_bool(image_version, false, "Show firmware version in Chromebox stored position.");
  DEFINE_bool(lock, true, "Make sure only 1 updater can be run during update");
  DEFINE_string(log_to, "", "Specify log file to write messages to.");
  brillo::FlagHelper::Init(argc, argv, "aver-updater");

  ConfigureLogging(FLAGS_log_to);

  if (FLAGS_lock && !LockUpdater()) {
    LOG(ERROR) << "There is another aver-updater running. Exiting now...";
    return 0;
  }

  AverStatus error = AverStatus::NO_ERROR;
  std::string video_version;
  std::string video_image_version;
  std::string version_info;

  std::unique_ptr<VideoDevice> aver_device(new VideoDevice(kAVerCAM520Pid));

  error = aver_device->OpenDevice();
  if (error != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to open the device.";
    return static_cast<int>(error);
  }

  if (FLAGS_device_version) {
    std::string device_version;
    aver_device->GetDeviceVersion(&device_version);
    if (error != AverStatus::NO_ERROR)
        return static_cast<int>(error);
    LOG(INFO) << "Firmware version on the device:" << device_version;
  }

  if (FLAGS_image_version) {
    std::string image_version;
    error = aver_device->GetImageVersion(&image_version);
    if (error != AverStatus::NO_ERROR)
      return static_cast<int>(error);
    LOG(INFO) << "Latest firmware available:" << image_version;
  }

  if (FLAGS_update) {
    error = aver_device->IsDeviceUpToDate(FLAGS_force);
    if (error != AverStatus::NO_ERROR) {
      if (error == AverStatus::OPEN_FOLDER_PATH_FAILED)
          LOG(ERROR) << "Failed to open file folder path.";
      else if (error == AverStatus::DEVICE_NOT_OPEN)
          LOG(ERROR) << "Failed to open Video Device.";
      else if (error == AverStatus::IO_CONTROL_OPERATION_FAILED)
          LOG(ERROR) << "Failed to do ioctl.";
      else if (error == AverStatus::DIR_CONTENT_ERR)
          LOG(ERROR) << "Failed to get directory content.";
      else if (error == AverStatus::FW_ALREADY_UPDATE)
          LOG(INFO) << "Firmware is up to date.";
      else
          LOG(ERROR) << "Unexpected error.";
      return static_cast<int>(error);
    }

    error = aver_device->LoadFirmwareToBuffer();
    if (error != AverStatus::NO_ERROR) {
      LOG(ERROR) << "Failed to load firmware to the buffer.";
      return static_cast<int>(error);
    }

    LOG(INFO) << "Updating device.";
    error = aver_device->PerformUpdate();
    if (error == AverStatus::NO_ERROR)
      LOG(INFO) << "Done. Updated firmware successfully.";
    else
      LOG(ERROR) << "Failed to update device.";
  }
  return static_cast<int>(error);
}