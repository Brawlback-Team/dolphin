#pragma once

#include "SlippiUtility.h"

using namespace SlippiUtility::Savestate;

// thank you Slippi :)

class BrawlbackSavestate
{

public:


    BrawlbackSavestate(u32 frame);
    ~BrawlbackSavestate();


    void Capture();
    void Load(std::vector<PreserveBlock> blocks);

    //static bool shouldForceInit;

    u32 frame = 0;

private:


    std::vector<ssBackupLoc> backupLocs = {};
    std::unordered_map<PreserveBlock, std::vector<u8>, preserve_hash_fn> preservationMap;
    std::vector<u8> dolphinSsBackup = {};

    void getDolphinState(PointerWrap& p);


    void initBackupLocs();


};
