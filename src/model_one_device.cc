// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "model_one_device.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <thread>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/logging.h>

#include "utilities.h"

namespace {
const char kVideoImagePath[] = "/lib/firmware/aver/";
const char kFwAudioName[] = "audio.";
const char kIspFileStartAck[] = "isp_file_start_ack";
const char kIspFileEndAck[] = "isp_file_end_ack";
const char kIspStartAck[] = "isp_start_ack";

constexpr unsigned int kHidVerOneMaxDataSize = 512;
constexpr unsigned int kReportIdCustomizeAck = 0x09;
constexpr unsigned int kCustomizedCmdIsp = 0x10;
constexpr unsigned int kAVerFirmwareCount = 12;
constexpr unsigned int kReceiveDataStartPosition = 2;
constexpr unsigned int kAVerAudioImageBlockSize = 508;
constexpr unsigned int kReportIdCustomizeCmd = 0x08;
constexpr unsigned int kReceiveStartIspStartPosCount = 4;
constexpr unsigned int kAVerXuOne = 5;
constexpr unsigned int kUvcxUcamFwVersion = 0x14;
constexpr unsigned int kRebootingWaitTwoMinutes = 120;
}  // namespace

ModelOneDevice::ModelOneDevice(const std::string& device_name,
                               const std::string& dev_path,
                               const std::string& fw_name_suffix) :
  UsbDevice(),
  device_name_(device_name),
  device_path_(dev_path),
  firmware_middle_name_(fw_name_suffix) {}

ModelOneDevice::~ModelOneDevice() {
  if (file_descriptor_hid_.is_valid())
    file_descriptor_hid_.reset();
}

AverStatus ModelOneDevice::SendHidVerOneControl(
  const struct ModelOneDeviceOutReport& customize_cmd) {
  int rt = HANDLE_EINTR(write(file_descriptor_hid_.get(),
                              &customize_cmd,
                              kHidVerOneMaxDataSize - 1));
  if (rt < 0) {
    LOG(ERROR) << "Write hid cmd failed.";
    return AverStatus::WRITE_DEVICE_FAILED;
  }

  struct ModelOneDeviceInReport hid_info;
  rt = HANDLE_EINTR(read(file_descriptor_hid_.get(),
                         &hid_info,
                         kHidVerOneMaxDataSize));
  if (rt < 0) {
    LOG(ERROR) << "Read hid msg failed.";
    return AverStatus::READ_DEVICE_FAILED;
  }

  if (hid_info.id != kReportIdCustomizeAck) {
    LOG(ERROR) << "Read hid msg failed";
    return AverStatus::READ_DEVICE_FAILED;
  }

  hid_return_msg_.clear();
  if (customize_cmd.isp_cmd != UVCX_UCAM_ISP_STATUS ||
      customize_cmd.isp_cmd != UVCX_UCAM_ISP_FILE_DNLOAD) {
    for (int i = 0; i < kHidVerOneMaxDataSize; i++)
      hid_return_msg_.push_back(hid_info.dat[i]);
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::FirmwareUpdate() {
  AverStatus status = IspStatusGet();
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Status Get failed.";
    return status;
  }

  status = IspFileStart();
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "File start failed.";
    return status;
  }

  status = IspFileDownload(firmware_buffer_,
                           firmware_buffer_.size());
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "File download failed.";
    return status;
  }

  status = IspFileEnd(firmware_buffer_.size());
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "File end failed.";
    return status;
  }

  status = IspStart();
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Isp start failed.";
    return status;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::IspStatusGet() {
  ModelOneDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_STATUS;
  AverStatus status = SendHidVerOneControl(customize_cmd);
  if (status != AverStatus::NO_ERROR)
    return status;

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::IspFileStart() {
  ModelOneDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_FILE_START;
  std::string fw_name;
  fw_name.append(firmware_version_);
  fw_name.insert(kAVerFirmwareCount, kFwAudioName);
  memcpy(customize_cmd.dat, fw_name.c_str(), fw_name.size());
  AverStatus status = SendHidVerOneControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send isp file start cmd.";
    return status;
  }

  bool ok = VerifyDeviceResponse(hid_return_msg_, kIspFileStartAck,
                                 kReceiveDataStartPosition);
  if (!ok) {
    LOG(ERROR) << "Failed to start isp upload follow in the device.";
    return AverStatus::ISP_FILE_START_HID_CMD_COMPARE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::IspFileDownload(const std::vector<char>& buffer,
    uint32_t isp_file_size) {
  ModelOneDeviceOutReport customize_cmd;
  uint32_t offset = 0;
  while (isp_file_size > 0) {
    unsigned int block_size =
      std::min<unsigned int>(isp_file_size, kAVerAudioImageBlockSize);
    memset(&customize_cmd, 0, sizeof(customize_cmd));
    customize_cmd.id = kReportIdCustomizeCmd;
    customize_cmd.report_cmd = kCustomizedCmdIsp;
    customize_cmd.isp_cmd = UVCX_UCAM_ISP_FILE_DNLOAD;
    std::copy(buffer.cbegin() + offset, buffer.cbegin() + offset + block_size,
              customize_cmd.dat);
    AverStatus status = SendHidVerOneControl(customize_cmd);
    if (status != AverStatus::NO_ERROR) {
      LOG(ERROR) << "Failed to send isp file to the device.";
      return status;
    }

    offset += block_size;
    isp_file_size -= block_size;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::IspFileEnd(uint32_t isp_file_size) {
  ModelOneDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_FILE_END;
  std::string fw_name;
  fw_name.append(firmware_version_);
  fw_name.insert(kAVerFirmwareCount, kFwAudioName);
  memcpy(customize_cmd.dat, fw_name.c_str(), fw_name.size());
  customize_cmd.dat[51] = 1;
  customize_cmd.dat[52] =
    static_cast<uint8_t>((isp_file_size & 0x000000FF) >> 0);
  customize_cmd.dat[53] =
    static_cast<uint8_t>((isp_file_size & 0x0000FF00) >> 8);
  customize_cmd.dat[54] =
    static_cast<uint8_t>((isp_file_size & 0x00FF0000) >> 16);
  customize_cmd.dat[55] =
    static_cast<uint8_t>((isp_file_size & 0xFF000000) >> 24);
  AverStatus status = SendHidVerOneControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send isp end cmd in firmware device.";
    return status;
  }

  bool ok = VerifyDeviceResponse(hid_return_msg_, kIspFileEndAck,
                                 kReceiveDataStartPosition);
  if (!ok) {
    LOG(ERROR)
        << "Failed to end isp file upload follow in the device.";
    return AverStatus::ISP_FILE_END_HID_CMD_COMPARE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::IspStart() {
  ModelOneDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_START;
  AverStatus status = SendHidVerOneControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to start update follow in firmware device.";
    return status;
  }

  bool ok = false;
  for (int i = 1; !ok && i < kReceiveStartIspStartPosCount; i++) {
    ok = VerifyDeviceResponse(hid_return_msg_,
                              kIspStartAck,
                              i);
  }

  if (!ok) {
    LOG(ERROR) << "Failed to start update follow in the device.";
    return AverStatus::ISP_START_HID_CMD_COMPARE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::OpenDevice() {
  bool ok = OpenUvcDevice(device_name_);
  if (!ok)
    return AverStatus::USB_UVC_NOT_FOUND;

  file_descriptor_hid_ = CreateFd(device_path_.c_str());
  if (!file_descriptor_hid_.is_valid()) {
    PLOG(ERROR) << "Open hid fd fail";
    return AverStatus::OPEN_HID_DEVICE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelOneDevice::GetDeviceVersion(std::string* device_version) {
  AverStatus status = GetXuControl(kAVerXuOne,
                                   kUvcxUcamFwVersion,
                                   device_version);
  return status;
}

AverStatus ModelOneDevice::LoadFirmwareToBuffer() {
  std::string image_version;
  AverStatus status = GetImageVersion(device_version_, &image_version);

  base::FilePath input_file_path(kVideoImagePath + image_version);
  // Need to get firmware name in temporary folder.
  if (base::PathExists(temp_path_)) {
    std::string img_ver;
    std::string ver = GetVersion(image_version);
    img_ver = ver + firmware_middle_name_;
    input_file_path = temp_path_.Append(img_ver);
  }

  if (!ReadFirmwareFileToBuffer(input_file_path, &firmware_buffer_))
    return AverStatus::READ_FW_TO_BUF_FAILED;

  return status;
}

AverStatus ModelOneDevice::PerformUpdate(const base::FilePath& tmp_path) {
  temp_path_ = tmp_path;
  AverStatus status = LoadFirmwareToBuffer();
  if (status != AverStatus::NO_ERROR)
    return status;

  status = FirmwareUpdate();
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Device update failed.";
    return status;
  }
  LOG(INFO) << "Device firmware updating. PLEASE WAIT DEVICE REBOOT.";
  std::this_thread::sleep_for(std::chrono::seconds(kRebootingWaitTwoMinutes));
  LOG(INFO) << "Isp update finish. Device will reboot!";
  return AverStatus::NO_ERROR;
}
