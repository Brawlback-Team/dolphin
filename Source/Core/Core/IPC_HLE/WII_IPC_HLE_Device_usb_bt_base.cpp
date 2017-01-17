// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/SysConf.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_base.h"

namespace IOS
{
namespace HLE
{
constexpr u16 BT_INFO_SECTION_LENGTH = 0x460;

void BackUpBTInfoSection(SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_SESSION_WIIROOT_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  if (File::Exists(filename))
    return;
  File::IOFile backup(filename, "wb");
  std::vector<u8> section(BT_INFO_SECTION_LENGTH);
  if (!sysconf->GetArrayData("BT.DINF", section.data(), static_cast<u16>(section.size())))
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to read source BT.DINF section");
    return;
  }
  if (!backup.WriteBytes(section.data(), section.size()))
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to back up BT.DINF section");
}

void RestoreBTInfoSection(SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_SESSION_WIIROOT_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  if (!File::Exists(filename))
    return;
  File::IOFile backup(filename, "rb");
  std::vector<u8> section(BT_INFO_SECTION_LENGTH);
  if (!backup.ReadBytes(section.data(), section.size()))
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to read backed up BT.DINF section");
    return;
  }
  sysconf->SetArrayData("BT.DINF", section.data(), static_cast<u16>(section.size()));
  File::Delete(filename);
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_base::CtrlMessage::CtrlMessage(const IOSIOCtlVRequest& ioctlv)
    : ios_request(ioctlv)
{
  request_type = Memory::Read_U8(ioctlv.in_vectors[0].address);
  request = Memory::Read_U8(ioctlv.in_vectors[1].address);
  value = Common::swap16(Memory::Read_U16(ioctlv.in_vectors[2].address));
  index = Common::swap16(Memory::Read_U16(ioctlv.in_vectors[3].address));
  length = Common::swap16(Memory::Read_U16(ioctlv.in_vectors[4].address));
  payload_addr = ioctlv.io_vectors[0].address;
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_base::CtrlBuffer::CtrlBuffer(const IOSIOCtlVRequest& ioctlv)
    : ios_request(ioctlv)
{
  m_endpoint = Memory::Read_U8(ioctlv.in_vectors[0].address);
  m_length = Memory::Read_U16(ioctlv.in_vectors[1].address);
  m_payload_addr = ioctlv.io_vectors[0].address;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_base::CtrlBuffer::FillBuffer(const u8* src,
                                                                      const size_t size) const
{
  _assert_msg_(WII_IPC_WIIMOTE, size <= m_length, "FillBuffer: size %li > payload length %i", size,
               m_length);
  Memory::CopyToEmu(m_payload_addr, src, size);
}
}  // namespace HLE
}  // namespace IOS
