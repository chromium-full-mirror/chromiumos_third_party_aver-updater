// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "model_two_device.h"

#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <thread>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/logging.h>

#include "utilities.h"

namespace {
const char kImagePath[5][24] = {"update/cx3uvc.img",
                                "update/RS_M12MO.bin",
                                "update/strange_100.bin",
                                "update/hawkeye_342.bin",
                                "update/boot_fprog.boot",
                               };
const char kFwVideoName[] = "video.dat";

constexpr unsigned int kAVerReadDeviceVersionMaxRetry = 3;
constexpr unsigned int kAVerReadDeviceVersionRetryIntervalMs = 500;
constexpr unsigned int kReportIdCustomizeCmd = 0x08;
constexpr unsigned int kMaxDataSize = 1024;
constexpr unsigned int kReadDataSize = 16;
constexpr unsigned int kAVerRebootingWaitSecond = 60;
constexpr unsigned int kMcuStrangeI2cAddr = (0x2c << 1);
constexpr unsigned int kMcuHawkeyeI2cAddr = (0x2e << 1);
constexpr unsigned int kHawkeyeFwChecksumCount = 256 * 1024 - 256;
constexpr unsigned int kFwFlowHawkeyeSelect = 0;
constexpr unsigned int kReportIdCustomizeAck = 0x09;
constexpr unsigned int kAVerDefaultImageBlockSize = 512;
constexpr unsigned int kDeviceFwNumFive = 5;
constexpr unsigned int kDeviceFwNumTwo = 2;
}  // namespace

ModelTwoDevice::ModelTwoDevice(const std::string& device_path)
  : UsbDevice(),
    device_path_(device_path),
    file_descriptor_(-1) {}


ModelTwoDevice::~ModelTwoDevice() {
  if (file_descriptor_.is_valid())
    file_descriptor_.reset();
}

AverStatus ModelTwoDevice::OpenDevice() {
  file_descriptor_ = CreateFd(device_path_.c_str());
  if (file_descriptor_ == -1) {
    PLOG(ERROR) << "Open video fd fail";
    return AverStatus::OPEN_HID_DEVICE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::GetDeviceVersion(std::string* device_version) {
  AverStatus status = AverStatus::UNKNOWN;
  std::string version;
  int attempts_count = 1;
  while (true) {
    status = ReadDeviceVersion();
    if (status == AverStatus::NO_ERROR ||
        attempts_count > kAVerReadDeviceVersionMaxRetry) {
      break;
    }

    LOG(ERROR) << "Failed to read device version, will retry.";
    attempts_count += 1;
    std::this_thread::sleep_for(
      std::chrono::milliseconds(kAVerReadDeviceVersionRetryIntervalMs));
  }

  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to read device version after "
               << kAVerReadDeviceVersionMaxRetry << " retries";
    return status;
  }

  device_version->assign(reinterpret_cast<char*>(&hid_info_.dat));

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::ReadDeviceVersion() {
  AverStatus status;

  ModelTwoDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.cmd = HID_CSTM_CMD_FACTORY_GET_FW_VERSION;
  customize_cmd.parm[0] = 0;
  customize_cmd.parm[1] = 0;
  status = SendHidVerTwoControl(customize_cmd);
  if (status != AverStatus::NO_ERROR)
    return status;

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::SendHidVerTwoControl(
  const struct ModelTwoDeviceOutReport& customize_cmd) {
  int rt = HANDLE_EINTR(write(file_descriptor_.get(),
                              &customize_cmd,
                              kMaxDataSize - 1));
  if (rt < 0) {
    LOG(ERROR) << "Write hid cmd failed.";
    return AverStatus::WRITE_DEVICE_FAILED;
  }

  rt = HANDLE_EINTR(read(file_descriptor_.get(), &hid_info_, kReadDataSize));
  if (rt < 0) {
    LOG(ERROR) << "Read hid msg failed.";
    return AverStatus::READ_DEVICE_FAILED;
  }

  if (hid_info_.id != kReportIdCustomizeAck) {
    LOG(ERROR) << "Read hid msg failed";
    return AverStatus::READ_DEVICE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::LoadFirmwareToBuffer() {
  AverStatus status = HandleCompressedFirmware();

  for (int i = 0; i < device_fw_num_; i++) {
    base::FilePath file_path = temp_path_.Append(kImagePath[i]);
    if (!ReadFirmwareFileToBuffer(file_path, &firmware_buffer_[i])) {
      base::DeletePathRecursively(temp_path_);
      return AverStatus::READ_FW_TO_BUF_FAILED;
    }
  }

  if (!base::DeletePathRecursively(temp_path_))
    LOG(ERROR) << "Failed to delete untar firmware.";

  return status;
}

AverStatus ModelTwoDevice::HandleCompressedFirmware() {
  // Check more device firmwares or not.
  bool ok = false;
  std::string video_fw_tmp;
  std::string ver = GetVersion(firmware_version_);
  video_fw_tmp = ver + kFwVideoName;
  ok = FindIntermediateCompressedFile(temp_path_, video_fw_tmp);
  if (ok) {
    base::FilePath video_file_path = temp_path_.Append(video_fw_tmp);
    ok = ExtractTarFile(temp_path_, video_file_path.value().c_str());
    device_fw_num_ = kDeviceFwNumFive;
  } else
    device_fw_num_ = kDeviceFwNumTwo;

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::PerformUpdate(const base::FilePath& tmp_path) {
  temp_path_ = tmp_path;
  AverStatus status = LoadFirmwareToBuffer();
  if (status != AverStatus::NO_ERROR)
    return status;

  status = FirmwareUpdate();
  return status;
}

AverStatus ModelTwoDevice::FirmwareUpdate() {
  AverStatus status;
  uint8_t cmd;
  uint32_t i2c_address;

  for (int i = 0; i < device_fw_num_; i++) {
    device_update_[i] = false;

    switch (i) {
      case M12MO_FW_UPLOAD:
        cmd = HID_CSTM_CMD_M12MO_COMPARE_CHECKSUM;
        i2c_address = 0;
        break;
      case HAWKEYE_FW_UPLOAD:
        cmd = HID_CSTM_CMD_FLASH_COMPARE_CHECKSUM;
        i2c_address = kMcuHawkeyeI2cAddr;
      case M051_FW_UPLOAD:
        cmd = HID_CSTM_CMD_FLASH_COMPARE_CHECKSUM;
        i2c_address = kMcuStrangeI2cAddr;
        break;
      case CX3_FW_UPLOAD:
      case BOOTIMG_FW_UPLOAD:
        device_update_[i] = true;
        continue;
        break;
    }

    status = IspCompareChecksum(cmd, i2c_address, 0, firmware_buffer_[i]);
    if (status == AverStatus::NO_ERROR) {
      LOG(INFO) << "Firmware checksum is not the same. Update Device:" << i;
      device_update_[i] = true;
    } else if (status == AverStatus::CHECKSUM_SAME) {
      LOG(INFO) << "Firmware checksum is the same. Skip device:" << i;
    } else if (status != AverStatus::NO_ERROR) {
      LOG(ERROR) << "Checksum failed. Device:" << i;
      return status;
    }
  }

  status = IspSupport();
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Isp support failed.";
    return status;
  }

  for (int i = 0; i < device_fw_num_; i++) {
    status = FirmwareUpload(i);
    if (status != AverStatus::NO_ERROR)
      return status;
  }

  status = IspUpdate();
  if (status != AverStatus::NO_ERROR)
    return status;

  LOG(INFO) << "Devices updating! No power off!";

  std::this_thread::sleep_for(
    std::chrono::seconds(kAVerRebootingWaitSecond * device_fw_num_));

  LOG(INFO) << "Isp update finish. Device will reboot!";
  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::IspCompareChecksum(uint8_t cmd,
    uint32_t i2c_address,
    uint32_t chip_select,
    const std::vector<char>& buffer) {
  checksum_ = 0;
  for (auto it = buffer.begin(); it != buffer.end(); it++) {
    if (i2c_address == kMcuHawkeyeI2cAddr &&
        distance(buffer.begin(), it) >= kHawkeyeFwChecksumCount)
      break;
    else
      checksum_ += static_cast<unsigned char>(*it);
  }

  ModelTwoDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.cmd = cmd;
  if (cmd == HID_CSTM_CMD_SAFE_ISP_UPLOAD_COMPARE_CHECKSUM) {
    customize_cmd.parm[0] = chip_select;
    customize_cmd.parm[1] = checksum_;
  } else {
    customize_cmd.parm[0] = checksum_;
    customize_cmd.parm[1] = i2c_address;
  }
  AverStatus status = SendHidVerTwoControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send hid command.";
    return status;
  }

  if (hid_info_.cmd == HID_ACK_COMPARE_SAME)
    return AverStatus::CHECKSUM_SAME;

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::IspSupport() {
  ModelTwoDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.cmd = HID_CSTM_CMD_SAFE_ISP_SUPPORT;
  AverStatus status = SendHidVerTwoControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send hid command.";
    return status;
  }

  if (hid_info_.cmd != HID_ACK_SAFE_ISP_SUPPORT) {
    LOG(ERROR) << "Can't support new firmware update progress.";
    return AverStatus::FAILED_SUPPORT_NEW_FW_UPDATE;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::IspUpdate() {
  ModelTwoDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.cmd = HID_CSTM_CMD_SAFE_ISP_UPDATA_START;
  customize_cmd.parm[0] = (device_update_[0] << 0) | (device_update_[1] << 1) |
                          (device_update_[2] << 2) | (device_update_[3] << 3);
  AverStatus status = SendHidVerTwoControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send hid command.";
    return status;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::FirmwareUpload(int index) {
  if (device_update_[index] == false)
    return AverStatus::NO_ERROR;

  AverStatus status = IspPrepare(HID_CSTM_CMD_SAFE_ISP_UPLOAD_PREPARE,
                                 index, 0, firmware_buffer_[index].size());
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Isp prepare failed.";
    return status;
  }

  status = IspErase(HID_CSTM_CMD_SAFE_ISP_ERASE_TEMP, index);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Isp erase failed.";
    return status;
  }

  status = IspUpload(HID_CSTM_CMD_SAFE_ISP_UPLOAD_TO_CX3 + index,
                     firmware_buffer_[index],
                     firmware_buffer_[index].size(),
                     kAVerDefaultImageBlockSize,
                     kFwFlowHawkeyeSelect);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Isp upload failed.";
    return status;
  }

  status = IspCompareChecksum(HID_CSTM_CMD_SAFE_ISP_UPLOAD_COMPARE_CHECKSUM, 0,
                              index, firmware_buffer_[index]);
  if (status == AverStatus::CHECKSUM_SAME) {
    LOG(INFO) << "Firmware in the flash is correct.";
    device_update_[index] = true;
  } else if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Checksum failed.";
    return status;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::IspPrepare(uint8_t cmd,
                                       uint32_t chip_select,
                                       uint32_t sector_count,
                                       uint32_t file_size) {
  ModelTwoDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.cmd = cmd;
  if (cmd == HID_CSTM_CMD_M051_FLASH_PREPARE ||
      cmd == HID_CSTM_CMD_SAFE_ISP_UPLOAD_PREPARE) {
    customize_cmd.parm[0] = chip_select;
    customize_cmd.parm[1] = file_size;
  } else
    customize_cmd.parm[0] = sector_count;
  AverStatus status = SendHidVerTwoControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send hid command. Command:" << cmd;
    return status;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::IspErase(uint8_t cmd, uint32_t chip_select) {
  ModelTwoDeviceOutReport customize_cmd;
  memset(&customize_cmd, 0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.cmd = cmd;
  customize_cmd.parm[0] = chip_select;
  AverStatus status = SendHidVerTwoControl(customize_cmd);
  if (status != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send hid command. Command:" << cmd;
    return status;
  }

  return AverStatus::NO_ERROR;
}

AverStatus ModelTwoDevice::IspUpload(uint8_t cmd,
                                      const std::vector<char>& buffer,
                                      uint32_t isp_file_size,
                                      uint32_t chip_block_size,
                                      uint32_t upload_select) {
  ModelTwoDeviceOutReport customize_cmd;
  uint32_t offset = 0;
  while (isp_file_size > 0) {
    unsigned int block_size =
      std::min<unsigned int>(isp_file_size, chip_block_size);

    memset(&customize_cmd, 0, sizeof(customize_cmd));
    customize_cmd.id = kReportIdCustomizeCmd;
    customize_cmd.cmd = cmd;
    customize_cmd.parm[0] = offset;
    customize_cmd.parm[1] = block_size;
    std::copy(buffer.cbegin() + offset, buffer.cbegin() + offset + block_size,
              customize_cmd.dat);
    AverStatus status = SendHidVerTwoControl(customize_cmd);
    if (status != AverStatus::NO_ERROR) {
      LOG(ERROR) << "Failed to send hid command. Command:" << cmd;
      return status;
    }

    offset += block_size;
    isp_file_size -= block_size;
  }

  return AverStatus::NO_ERROR;
}
