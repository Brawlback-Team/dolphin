#pragma once

#include "SlippiUtility.h"

using namespace SlippiUtility::Savestate;

// thank you Slippi :)

class BrawlbackSavestate
{

public:


    BrawlbackSavestate();
    ~BrawlbackSavestate();


    void Capture();
    void Load(std::vector<PreserveBlock> blocks);

    //static bool shouldForceInit;

private:


    std::vector<ssBackupLoc> backupLocs = {};
    std::unordered_map<PreserveBlock, std::vector<u8>, preserve_hash_fn> preservationMap;
    std::vector<u8> dolphinSsBackup = {};

    void getDolphinState(PointerWrap& p);


    void initBackupLocs();

    //std::thread firstHalf;
    //std::thread secondHalf;


};
