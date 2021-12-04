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
  struct preserve_hash_fn
  {
    std::size_t operator()(const PreserveBlock& node) const
    {
      return node.address ^ node.length;  // TODO: This is probably a bad hash
    }
  };


  std::vector<ssBackupLoc> backupLocs = {};
  std::unordered_map<PreserveBlock, std::vector<u8>, preserve_hash_fn> preservationMap;


  void initBackupLocs();


};
