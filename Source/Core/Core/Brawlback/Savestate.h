#pragma once

#include "Common/CommonTypes.h"
#include <unordered_map>

// thank you Slippi :)

class BrawlbackSavestate
{

public:

  struct PreserveBlock
  {
    u32 address;
    u32 length;

    bool operator==(const PreserveBlock& p) const
    {
      return address == p.address && length == p.length;
    }
  };

  BrawlbackSavestate();
  ~BrawlbackSavestate();


  void Capture();
  void Load(std::vector<PreserveBlock> blocks);


private:

  typedef struct
  {
    u32 startAddress;
    u32 endAddress;
    u8* data;
  } ssBackupLoc;


  std::vector<ssBackupLoc> backupLocs = {};


  void initBackupLocs();


};
