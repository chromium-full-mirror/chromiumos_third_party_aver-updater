// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "video_device.h"

#include <fcntl.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <sys/ioctl.h>
#include <thread>

#include <base/files/file_util.h>
#include "utilities.h"

namespace {
const char kIspFileStartAck[] = "isp_file_start_ack";
const char kIspFileEndAck[] = "isp_file_end_ack";
const char kIspStartAck[] = "isp_start_ack";
const char kIspFileName[] = "isp.dat";

const char kVideoImagePath[]  = "/lib/firmware/aver/";
const char kDefaultVideoDeviceMountPoint[] = "/sys/class/video4linux";
const char kDefaultHidDeviceMountPoint[] = "/sys/class/hidraw";
const char kAverCAM520FirmwareNum[] = "0.0.0018.";
const char kAverCAM520Name[] = "CAM520";
const char kAverVC520Name[] = "VC520";

constexpr unsigned int kAverVendorID = 0x2574;
constexpr unsigned int kAverCAM520ProductID = 0x0910;
constexpr unsigned int kAverVC520ProductID = 0x0901;

constexpr unsigned int kMaxDataSize = 512;

constexpr unsigned int kUvcxUcamFwVersion    = 0x14;
constexpr unsigned int kCustomizedCmdIsp     = 0x10;
constexpr unsigned int kReportIdCustomizeCmd = 0x08;
constexpr unsigned int kReportIdCustomizeAck = 0x09;

constexpr unsigned int kDefaultUvcGetLenQueryControlSize = 2;
constexpr unsigned int kAVerDefaultImageBlockSize = 508;
constexpr unsigned int kAVerRebootingWaitSecond = 180;
constexpr unsigned int kAVerReadDeviceVersionRetryIntervalMs = 500;
constexpr unsigned int kAVerReadDeviceVersionMaxRetry = 3;
constexpr unsigned int kAVerXuOne = 5;

/**
 * @brief report (input) (to pc)
 */
struct InReport
{
  uint8_t id;                         /**< report id */
  uint8_t dat[512 - sizeof(uint8_t)]; /**< report data */
};
} // namespace


VideoDevice::VideoDevice(const std::string& pid)
    : usb_pid_(pid), file_descriptor_(-1), file_descriptor_hid_(-1), is_open_(false) {
}

VideoDevice::~VideoDevice() {
  CloseDevice();
}

std::string VideoDevice::FindDevice() {
  std::vector<std::string> contents;
  bool get_ok = GetDirectoryContents(kDefaultVideoDeviceMountPoint, &contents);
  if (!get_ok)
    return std::string();

  for (auto const& content : contents) {
    if (content.compare(".") == 0 || content.compare("..") == 0)
      continue;

    std::string video_dir =
        std::string(kDefaultVideoDeviceMountPoint) + "/" + content;

    std::string name_path = video_dir + "/name";
    std::string name;

    base::FilePath input_file_path(name_path.c_str());
    base::ReadFileToString(input_file_path, &name);

    if (name.find(kAverVC520Name) != name.npos ||
        name.find(kAverCAM520Name) != name.npos)
      return "/dev/" + content;
  }
  return std::string();
}

std::string VideoDevice::FindHidDevice() {
  std::vector<std::string> contents;
  bool get_ok = GetDirectoryContents(kDefaultHidDeviceMountPoint, &contents);
  if (!get_ok)
    return std::string();

  for (auto const& content : contents) {
    if (content.compare(".") == 0 || content.compare("..") == 0)
      continue;

    std::string hid_dir =
        std::string(kDefaultHidDeviceMountPoint) + "/" + content;

    std::string vid_path = hid_dir + "/device/../../idVendor";
    std::string vid;

    std::string pid_path = hid_dir + "/device/../../idProduct";
    std::string pid;

    if (!ReadFileContent(vid_path, &vid))
      continue;

    int vidnum = 0;
    if (!ConvertHexStringToInt(vid, &vidnum))
      continue;

    if (!ReadFileContent(pid_path, &pid))
      continue;

    int pidnum = 0;
    if (!ConvertHexStringToInt(pid, &pidnum))
      continue;

    if (vidnum == kAverVendorID &&
       (pidnum == kAverCAM520ProductID || pidnum == kAverVC520ProductID))
      return "/dev/" + content;

  }

  return std::string();
}

AverStatus VideoDevice::OpenDevice() {
  std::string dev_path = FindDevice();
  if (dev_path.empty())
    return AverStatus::USB_PID_NOT_FOUND;

  int fd = open(dev_path.c_str(), O_RDWR, 0);
  if (fd == -1) {
    PLOG(ERROR) << "Open video fd fail";
    return AverStatus::OPEN_DEVICE_FAILED;
  }
  file_descriptor_ = fd;

  std::string hid_dev_path = FindHidDevice();
  if (hid_dev_path.empty())
    return AverStatus::USB_HID_NOT_FOUND;

  int fd_hid = open(hid_dev_path.c_str(), O_RDWR);
  if (fd_hid == -1) {
    PLOG(ERROR) << "Open hid fd fail";
    return AverStatus::OPEN_HID_DEVICE_FAILED;
  }
  file_descriptor_hid_ = fd_hid;

  is_open_ = true;
  return AverStatus::NO_ERROR;
}

void VideoDevice::CloseDevice() {
  if (file_descriptor_ >= 0)
    close(file_descriptor_);

  if (file_descriptor_hid_ >= 0)
    close(file_descriptor_hid_);

  file_descriptor_ = -1;
  file_descriptor_hid_ = -1;
  is_open_ = false;
}

AverStatus VideoDevice::ReadDeviceVersion(std::string* device_version) {
  AverStatus error = GetXuControl(kAVerXuOne, kUvcxUcamFwVersion, device_version);
  if (error != AverStatus::NO_ERROR)
    return error;

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::GetImageVersion(std::string* fw_version) {
  if (!image_version_.empty()) {
    *fw_version = image_version_;
    return AverStatus::NO_ERROR;
  }

  std::vector<std::string> files;
  bool get_ok = GetDirectoryContents(kVideoImagePath, &files);
  if (!get_ok)
    return AverStatus::DIR_CONTENT_ERR;

  auto selected_file_it = files.end();
  for (auto it = files.begin(); it != files.end(); it++) {
    if (it->substr(0, sizeof(kAverCAM520FirmwareNum) - 1) != kAverCAM520FirmwareNum) {
      continue;
    }

    if (selected_file_it == files.end() || *selected_file_it < *it) {
      selected_file_it = it;
    }
  }

  if (selected_file_it == files.end())
    return AverStatus::DIR_CONTENT_ERR;

  *fw_version = image_version_ = *selected_file_it;
  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::PerformUpdate() {
  AverStatus error = IspStatusGet();
  if (error != AverStatus::NO_ERROR)
    return error;

  error = IspFileStart();
  if (error != AverStatus::NO_ERROR)
    return error;

  error = IspFileDownload(firmware_buffer_, firmware_buffer_.size());
  if (error != AverStatus::NO_ERROR)
    return error;

  error = IspFileEnd(firmware_buffer_.size());
  if (error != AverStatus::NO_ERROR)
    return error;

  error = IspStart();
  if (error != AverStatus::NO_ERROR)
    return error;

  LOG(INFO) << "Isp updating! PLEASE WAIT CAM520 REBOOT.";
  std::this_thread::sleep_for(std::chrono::seconds(kAVerRebootingWaitSecond));
  LOG(INFO) << "Isp update finish. CAM520 will reboot!";

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::GetXuControl(unsigned char unit_id,
                              unsigned char control_selector,
                              std::string* data) {
  int data_len;
  AverStatus error = QueryDataSize(unit_id, control_selector, &data_len);
  if (error != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Query data size failed.";
    return error;
  }

  uint8_t query_data[data_len];
  memset(query_data, '\0', sizeof(query_data));

  struct uvc_xu_control_query control_query;
  control_query.unit = unit_id;
  control_query.selector = control_selector;
  control_query.query = UVC_GET_CUR;
  control_query.size = data_len;
  control_query.data = query_data;
  int err = HANDLE_EINTR(ioctl(file_descriptor_, UVCIOC_CTRL_QUERY, &control_query));
  if (err < 0) {
    LOG(ERROR) << "Uvc ioctl query failed.";
    return AverStatus::IO_CONTROL_OPERATION_FAILED;
  }

  for (int i = 0; i < data_len; i++)
    data->push_back(query_data[i]);

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::SendHidControl(const struct OutReport &customize_cmd) {
  struct InReport *hid_info;

  CHECK(is_open_) << "Attempt to send HID command while the device is closed.";

  int rt = HANDLE_EINTR(write(file_descriptor_hid_,
                              &customize_cmd, kMaxDataSize - 1));
  if (rt < 0) {
      LOG(ERROR) << "Write hid cmd failed.";
      return AverStatus::WRITE_DEVICE_FAILED;
  }

  char hid_read_buf[kMaxDataSize];
  rt = HANDLE_EINTR(read(file_descriptor_hid_, hid_read_buf, kMaxDataSize));
  if (rt < 0) {
      LOG(ERROR) << "Read hid msg failed.";
      return AverStatus::READ_DEVICE_FAILED;
  }

  hid_info = reinterpret_cast<struct InReport*>(&hid_read_buf);
  if (hid_info->id != kReportIdCustomizeAck) {
    LOG(ERROR) << "Read hid msg failed";
    return AverStatus::READ_DEVICE_FAILED;
  }

  hid_return_msg_.clear();
  if (customize_cmd.isp_cmd != UVCX_UCAM_ISP_STATUS ||
      customize_cmd.isp_cmd != UVCX_UCAM_ISP_FILE_DNLOAD) {
    for (int i = 0; i < kMaxDataSize; i++)
      hid_return_msg_.push_back(hid_read_buf[i]);
  }

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::QueryDataSize(unsigned char unit_id,
                               unsigned char control_selector,
                               int* data_size) {
  uint8_t size_data[kDefaultUvcGetLenQueryControlSize];
  struct uvc_xu_control_query size_query;
  size_query.unit = unit_id;
  size_query.selector = control_selector;
  size_query.query = UVC_GET_LEN;
  size_query.size = kDefaultUvcGetLenQueryControlSize;
  size_query.data = size_data;
  int error = HANDLE_EINTR(ioctl(file_descriptor_, UVCIOC_CTRL_QUERY, &size_query));
  if (error < 0)
    return AverStatus::IO_CONTROL_OPERATION_FAILED;

  int size = (size_data[1] << 8) | (size_data[0]);
  *data_size = size;
  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::IsDeviceUpToDate(bool force) {
  AverStatus error = GetDeviceVersion(&device_version_);
  if (error != AverStatus::NO_ERROR)
    return error;

  LOG(INFO) << "Firmware version on the device:" << device_version_;

  error = GetImageVersion(&image_version_);
  if (error != AverStatus::NO_ERROR)
    return error;

  LOG(INFO) << "Latest firmware available:" << image_version_;

  if (force)
    return AverStatus::NO_ERROR;

  if (CompareVersions(device_version_, image_version_) < 0)
    return AverStatus::NO_ERROR;

  return AverStatus::FW_ALREADY_UPDATE;
}

AverStatus VideoDevice::GetDeviceVersion(std::string* device_version) {
  AverStatus error = AverStatus::UNKNOWN;
  std::string version;

  int attempts_count = 1;
  while (true) {
    error = ReadDeviceVersion(&version);
    if (error == AverStatus::NO_ERROR ||
        attempts_count > kAVerReadDeviceVersionMaxRetry) {
      break;
    }

    LOG(ERROR) << "Failed to read device version, will retry.";
    attempts_count += 1;
    std::this_thread::sleep_for(
      std::chrono::milliseconds(kAVerReadDeviceVersionRetryIntervalMs));
  }

  if (error != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to read device version after " <<
                   kAVerReadDeviceVersionMaxRetry << " retries";
    return error;
  }

  *device_version = version;
  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::IspStatusGet() {
  OutReport customize_cmd;
  memset(&customize_cmd, 0x0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_STATUS;
  AverStatus error = SendHidControl(customize_cmd);
  if (error != AverStatus::NO_ERROR)
    return error;

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::IspFileStart() {
  OutReport customize_cmd;
  memset(&customize_cmd, 0x0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_FILE_START;
  memcpy(customize_cmd.dat, kIspFileName, sizeof(kIspFileName));

  AverStatus error = SendHidControl(customize_cmd);
  if (error != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send isp file start cmd in CAM520. Error code: "
               << error;
    return error;
  }

  bool ok = VerifyDeviceResponse(hid_return_msg_, kIspFileStartAck);
  if (!ok) {
    LOG(ERROR) << "Failed to start isp upload follow in CAM520.";
    return AverStatus::ISP_FILE_START_HID_CMD_COMPARE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::IspFileDownload(std::vector<char> buffer, uint32_t isp_file_size) {
  OutReport customize_cmd;
  uint32_t offset = 0;
  while (isp_file_size > 0) {
    unsigned int block_size =
        std::min<unsigned int>(isp_file_size, kAVerDefaultImageBlockSize);

    memset(&customize_cmd, 0x0, sizeof(customize_cmd));
    customize_cmd.id = kReportIdCustomizeCmd;
    customize_cmd.report_cmd = kCustomizedCmdIsp;
    customize_cmd.isp_cmd = UVCX_UCAM_ISP_FILE_DNLOAD;
    std::copy(buffer.cbegin() + offset, buffer.cbegin() + offset + block_size, customize_cmd.dat);
    AverStatus error = SendHidControl(customize_cmd);
    if (error != AverStatus::NO_ERROR) {
      LOG(ERROR) << "Failed to send isp file to CAM520.";
      return error;
    }

    offset += block_size;
    isp_file_size -= block_size;
  }

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::IspFileEnd(uint32_t isp_file_size) {
  OutReport customize_cmd;
  memset(&customize_cmd, 0x0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_FILE_END;
  memcpy(customize_cmd.dat, kIspFileName, sizeof(kIspFileName));
  customize_cmd.dat[51] = 1;
  customize_cmd.dat[52] = static_cast<uint8_t>((isp_file_size & 0x000000FF) >> 0);
  customize_cmd.dat[53] = static_cast<uint8_t>((isp_file_size & 0x0000FF00) >> 8);
  customize_cmd.dat[54] = static_cast<uint8_t>((isp_file_size & 0x00FF0000) >> 16);
  customize_cmd.dat[55] = static_cast<uint8_t>((isp_file_size & 0xFF000000) >> 24);
  AverStatus error = SendHidControl(customize_cmd);
  if (error != AverStatus::NO_ERROR) {
    LOG(ERROR) << "Failed to send isp file end cmd in CAM520. Error code: "
               << error;
    return error;
  }

  bool ok = VerifyDeviceResponse(hid_return_msg_, kIspFileEndAck);
  if (!ok) {
    LOG(ERROR) << "Failed to end isp file upload follow in CAM520.";
    return AverStatus::ISP_FILE_END_HID_CMD_COMPARE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::IspStart() {
  OutReport customize_cmd;
  memset(&customize_cmd, 0x0, sizeof(customize_cmd));
  customize_cmd.id = kReportIdCustomizeCmd;
  customize_cmd.report_cmd = kCustomizedCmdIsp;
  customize_cmd.isp_cmd = UVCX_UCAM_ISP_START;
  AverStatus error = SendHidControl(customize_cmd);
  if (error != AverStatus::NO_ERROR) {
      LOG(ERROR) << "Failed to start isp update follow in CAM520.";
      return error;
  }

  bool ok = VerifyDeviceResponse(hid_return_msg_, kIspStartAck);
  if (!ok) {
    LOG(ERROR) << "Failed to start isp update follow in CAM520.";
    return AverStatus::ISP_START_HID_CMD_COMPARE_FAILED;
  }

  return AverStatus::NO_ERROR;
}

AverStatus VideoDevice::LoadFirmwareToBuffer(){
  std::string fw_num;
  AverStatus error = GetImageVersion(&fw_num);
  if (error != AverStatus::NO_ERROR)
    return error;

  std::string file_path = kVideoImagePath;
  file_path += fw_num;
  base::FilePath input_file_path(file_path.c_str());
  std::vector<char> buffer;
  if (!ReadFirmwareFileToBuffer(input_file_path, &firmware_buffer_))
    return AverStatus::READ_FW_TO_BUF_FAILED;

  return AverStatus::NO_ERROR;
}