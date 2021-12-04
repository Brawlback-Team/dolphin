#include "Savestate.h"

#include <Core/HW/Memmap.h>
#include "common/Logging/Log.h"
#include <Common/MemoryUtil.h>

BrawlbackSavestate::BrawlbackSavestate()
{
  initBackupLocs();
  for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
  {
    auto size = it->endAddress - it->startAddress;
    it->data = static_cast<u8*>(Common::AllocateAlignedMemory(size, 64));
  }
}

BrawlbackSavestate::~BrawlbackSavestate()
{
  for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
  {
    Common::FreeAlignedMemory(it->data);
  }
}

void BrawlbackSavestate::initBackupLocs()
{
  // lol this is literally all of mem1 and mem2 right now. It stutters hard when its done every
  // frame but for testing purposes it works. 
  static std::vector<ssBackupLoc> backupRegions = { 
    {0x805b5160, 0x817da5a0, nullptr},
    {0x90000800, 0x935e0000, nullptr},
  };

  backupLocs.insert(backupLocs.end(), backupRegions.begin(), backupRegions.end());
}

void BrawlbackSavestate::Capture()
{
  for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
  {
    auto size = it->endAddress - it->startAddress;
    Memory::CopyFromEmu(it->data, it->startAddress, size);
  }
}

void BrawlbackSavestate::Load(std::vector<PreserveBlock> blocks)
{
  INFO_LOG(BRAWLBACK, "Savestate loading not implemented yet");
}
