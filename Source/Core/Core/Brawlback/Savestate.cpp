#include "Savestate.h"

#include <Core/HW/Memmap.h>
#include "common/Logging/Log.h"
#include <Common/MemoryUtil.h>
#include <Core/HW/EXI/EXI.h>

#define LOW_BOUND_MEM 0x80000000


// lots of code here is heavily derived from Slippi's Savestates.cpp



BrawlbackSavestate::BrawlbackSavestate(u32 frame)
{
    this->frame = frame;
    
    // init member list with proper addresses
    initBackupLocs();

    //u8 *ptr = nullptr;
    //PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);

    //getDolphinState(p);
    //const size_t buffer_size = reinterpret_cast<size_t>(ptr);
    //dolphinSsBackup.resize(buffer_size);

    // iterate through address ranges and allocate mem for our savestates
    for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
    {
        auto size = it->endAddress - it->startAddress;
        if (it->endAddress < LOW_BOUND_MEM)
        {
            size = it->endAddress;
        }
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


void BrawlbackSavestate::getDolphinState(PointerWrap& p)
{
    // p.DoArray(Memory::m_pRAM, Memory::RAM_SIZE);
    // p.DoMarker("Memory");
    // VideoInterface::DoState(p);
    // p.DoMarker("VideoInterface");
    // SerialInterface::DoState(p);
    // p.DoMarker("SerialInterface");
    // ProcessorInterface::DoState(p);
    // p.DoMarker("ProcessorInterface");
    // DSP::DoState(p);
    // p.DoMarker("DSP");
    // DVDInterface::DoState(p);
    // p.DoMarker("DVDInterface");
    // GPFifo::DoState(p);
    // p.DoMarker("GPFifo");
    ExpansionInterface::DoState(p);
    p.DoMarker("ExpansionInterface");
    // AudioInterface::DoState(p);
    // p.DoMarker("AudioInterface");
}

void BrawlbackSavestate::initBackupLocs()
{
    static std::vector<ssBackupLoc> allMem = {
        //{0x805b5160, 0x817da5a0, nullptr},  // all of mem1
        //{0x90000800, 0x935e0000, nullptr},  // all of mem2
        {0x805b5160, 0x935e0000, nullptr},
    };

    static std::vector<ssBackupLoc> fullBackupRegions = {
        //{0x800064E0, 0x805A5154, nullptr}, // Data 0-7 && bss

        
        // mem1
        //{0x805b5160, 0x805ca260, nullptr }, // System FW
        {0x80611f60, 0x80673460, nullptr }, // System
        {0x80b8db60, 0x80c23a60, nullptr }, // Effect
        {0x805d1e60, 0x80611f60, nullptr }, // RenderFifo
        {0x80c23a60, 0x80da3a60, nullptr }, // InfoResource
        {0x80da3a60, 0x80fd6260, nullptr }, // CommonResource
        {0x81049e60, 0x81061060, nullptr }, // Tmp
        //{0x8154e560, 0x81601960, nullptr }, // Physics
        //{0x81382b60, 0x814ce460, nullptr }, // ItemInstance
        {0x814ce460, 0x8154e560, nullptr }, // StageInstance
        {0x8123ab60, 0x8128cb60, nullptr }, // Fighter1Instance
        {0x8128cb60, 0x812deb60, nullptr }, // Fighter2Instance
        {0x812deb60, 0x81330b60, nullptr }, // Fighter3Instance
        {0x81330b60, 0x81382b60, nullptr }, // Fighter4Instance
        {0x81601960, 0x81734d60, nullptr }, // InfoInstance
        {0x81734d60, 0x817ce860, nullptr }, // MenuInstance
        //{0x80fd6260, 0x81049e60, nullptr }, // MeleeFont
        //{0x80673460, 0x80b8db60, nullptr }, // OverlayCommon
        //{0x810f1a60, 0x81162560, nullptr }, // OverlayStage
        //{0x81162560, 0x811aa160, nullptr }, // OverlayMenu
        //{0x81061060, 0x810a9560, nullptr }, // OverlayFighter1
        //{0x810a9560, 0x810f1a60, nullptr }, // OverlayFighter2
        //{0x811aa160, 0x811f2660, nullptr }, // OverlayFighter3
        //{0x811f2660, 0x8123ab60, nullptr }, // OverlayFighter4
        //{0x805ca260, 0x805d1e60, nullptr }, // Thread

        // mem2
        //{0x90199800, 0x90e61400, nullptr }, // Sound
        //{0x90e77500, 0x90fddc00, nullptr }, // Network
        {0x90e61400, 0x90e77500, nullptr }, // WiiPad
        //{0x91018b00, 0x91301b00, nullptr }, // IteamResource
        //{0x91301b00, 0x9134cc00, nullptr }, // Replay
        {0x92f34700, 0x9359ae00, nullptr }, // StageResoruce
        {0x9151fa00, 0x91a72e00, nullptr }, // Fighter1Resoruce
        {0x91b04c80, 0x92058080, nullptr }, // Fighter2Resoruce
        {0x920e9f00, 0x9263d300, nullptr }, // Fighter3Resoruce
        {0x926cf180, 0x92c22580, nullptr }, // Fighter4Resoruce
        {0x91a72e00, 0x91b04c80, nullptr }, // Fighter1Resoruce2
        {0x92058080, 0x920e9f00, nullptr }, // Fighter2Resoruce2
        {0x9263d300, 0x926cf180, nullptr }, // Fighter3Resoruce2
        {0x92c22580, 0x92cb4400, nullptr }, // Fighter4Resoruce2
        {0x91478e00, 0x914d2900, nullptr }, // FighterEffect
        {0x92cb4400, 0x92dcdf00, nullptr }, // FighterTechqniq
        //{0x914d2900, 0x914ec400, nullptr }, // FighterKirbyResource1
        //{0x914ec400, 0x91505f00, nullptr }, // FighterKirbyResource2
        //{0x91505f00, 0x9151fa00, nullptr }, // FighterKirbyResource3
        //{0x92dcdf00, 0x92e34600, nullptr }, // AssistFigureResource
        //{0x9359ae00, 0x935ce200, nullptr }, // ItemExtraResource
        //{0x92e34600, 0x92f34700, nullptr }, // PokemonResource
        {0x9134cc00, 0x91478e00, nullptr }, // CopyFB
        {0x90167400, 0x90199800, nullptr }, // GameGlobal
        //{0x90fddc00, 0x91018b00, nullptr }, // GlobalMode

        // data sections
        {0x800064E0, 0x805A1320+0x3E00, nullptr}, // Data 0-7
        {0x80494880, 0x80494880+0x1108D4, nullptr}, // bss


        

        // based off Fracture's SaveStates.cpp
        // https://github.com/Fracture17/PowerPC-Assembly-Functions/blob/master/PowerPC%20Assembly%20Functions/Save%20States.cpp

        /*
        {0x91c0ac84, 0x91c0ac98, nullptr},  // copy modules from 0x91c0ac84 to 0x91c0ac98
        {0x91c0ac84, 0x14, nullptr},        // save animation object ptr things (is the same mem as line above)
        
        //{0x814ce460, 0x80100, nullptr},  //stage
        //{0x812d39f4 - 0x20, 4, nullptr}, // ??

        {0x901AE000 + 0x870*0, 0x2C8, nullptr}, // save varaibles pt1
        {0x901AE000 + 0x870*1, 0x2C8, nullptr}, // save varaibles pt2
        {0x901AE000 + 0x870*2, 0x2C8, nullptr}, // save varaibles pt3
        {0x901AE000 + 0x870*3, 0x2C8, nullptr}, // save varaibles pt4

        {0x8062fb40, 0x806312ac, nullptr}, // save system thing
        {0x80b879b4, 4, nullptr}, // save FAT index
        {0x80b8516c, 0x80b8545c, nullptr}, // save other modules
        {0x806312f0, 0x8063ce70, nullptr}, // save subaction table thing
        {0x8128cb60, 0x94, nullptr}, // save instance (new)
        */
    };

    static std::vector<ssBackupLoc> test = {
        // data sections
        //{0x800064E0, 0x805A5154, nullptr}, // Data 0-7 && bss

        // mem1
        //{0x805b5160, 0x805ca260, nullptr}, // System FW
        {0x80611f60, 0x80673460, nullptr}, // System
        {0x80b8db60, 0x80c23a60, nullptr}, // Effect
        //{0x805d1e60, 0x80611f60, nullptr}, // RenderFifo
        //{0x8154e560, 0x81601960, nullptr}, // Physics
        //{0x805ca260, 0x805d1e60, nullptr}, // Thread

        {0x8123ab60, 0x8128cb60, nullptr}, // Fighter1Instance
        {0x8128cb60, 0x812deb60, nullptr}, // Fighter2Instance
        {0x80c23a60, 0x80da3a60, nullptr}, // InfoResource
        {0x80da3a60, 0x80fd6260, nullptr}, // CommonResource
        {0x815edf60, 0x817bad60, nullptr}, // InfoExtraResource
        {0x81601960, 0x81734d60, nullptr}, // InfoInstance
        {0x80673460, 0x80b8db60, nullptr}, // OverlayCommon
        //{0x81061060, 0x810a9560, nullptr}, // OverlayFighter1
        //{0x810a9560, 0x810f1a60, nullptr}, // OverlayFighter2
        //{0x81049e60, 0x81061060, nullptr}, // Tmp

        // mem2
        //{0x90199800, 0x90e61400, nullptr}, // Sound
        {0x90e61400, 0x90e77500, nullptr}, // WiiPad
        //{0x91018b00, 0x91301b00, nullptr}, // IteamResource (is this for items, or for teams?)
        //{0x91301b00, 0x9134cc00, nullptr}, // Replay
        {0x9151fa00, 0x91a72e00, nullptr}, // Fighter1Resource
        {0x91b04c80, 0x92058080, nullptr}, // Fighter2Resource
        {0x91a72e00, 0x91b04c80, nullptr}, // Fighter1Resource2
        {0x91478e00, 0x914d2900, nullptr}, // Fighter2Resource2
        {0x92cb4400, 0x92dcdf00, nullptr}, // FighterTechqniq
        {0x9134cc00, 0x91478e00, nullptr}, // CopyFB
        {0x90167400, 0x90199800, nullptr}, // GameGlobal
        //{0x90fddc00, 0x91018b00, nullptr}, // GlobalMode
    };

    static std::vector<ssBackupLoc> positionTesting = { // only overwrites p1/p2 x&y pos
        {0x8126F658, 0x8126F658+1, nullptr},
        {0x8126F65c, 0x8126F65c+1, nullptr},

        {0x812c1a78, 0x812c1a78+1, nullptr},
        {0x812d1a7c, 0x812d1a7c+1, nullptr},
    };

    // wip
    static std::vector<PreserveBlock> excludeSections = {
        //{0x90e61400, 0x90e77500-0x90e61400}, // WiiPad

        // one of these two fixed it (not being able to do inputs after rollback)
        //{0x805b5160, 0x805ca260-0x805b5160}, // System FW
        //{0x805ca260, 0x805d1e60 - 0x805ca260},  // Thread

        // infinite loop fixes
        //{0x8062fb40, 0x4ea},  // clearGeneralTerm (probably not correct)
        //{0x805b62a0+0x8c, 4},  // HSD_PadRumbleActiveAll
        //{0x805A0068, 0x17c},   // gfTaskScheduler (in gfTaskScheduler::process)
    };

    SlippiInitBackupLocations(this->backupLocs, allMem, excludeSections);
    //SlippiInitBackupLocations(this->backupLocs, fullBackupRegions, excludeSections);
    //SlippiInitBackupLocations(this->backupLocs, test, excludeSections);
    //SlippiInitBackupLocations(this->backupLocs, positionTesting, excludeSections);
  
}

void BrawlbackSavestate::Capture()
{

    // copy game mem
    for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
    {
        auto size = it->endAddress - it->startAddress;
        // since no addresses will be lower than this, if this is the case, the backupLoc endAddress
        // represents a size rather than an end address
        if (it->endAddress < LOW_BOUND_MEM) 
        {
            Memory::CopyFromEmu(it->data, it->startAddress, it->endAddress);
        }
        else
        {
            Memory::CopyFromEmu(it->data, it->startAddress, size);  // game -> emu
        }
    }

    // copy dolphin states
    //u8 *ptr = &dolphinSsBackup[0];
    //PointerWrap p(&ptr, PointerWrap::MODE_WRITE);
    //getDolphinState(p);

}

void BrawlbackSavestate::Load(std::vector<PreserveBlock> blocks)
{

    // Back up regions of game that should stay the same between savestates

    for (auto it = blocks.begin(); it != blocks.end(); ++it)
    {
        if (!preservationMap.count(*it)) // if this PreserveBlock is NOT in our preservationMap
        {
            // TODO: Clear preservation map when game ends
            preservationMap[*it] = std::vector<u8>(it->length); // init new entry at this PreserveBlock key
        }

        Memory::CopyFromEmu(&preservationMap[*it][0], it->address, it->length);
    }


    // Restore memory blocks
    for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
    {
        auto size = it->endAddress - it->startAddress;
        if (it->endAddress < LOW_BOUND_MEM)
        {
            Memory::CopyToEmu(it->startAddress, it->data, it->endAddress);  // emu -> game
        }
        else
        {
            Memory::CopyToEmu(it->startAddress, it->data, size);  // emu -> game
        }
    }

    //// Restore audio
    //u8 *ptr = &dolphinSsBackup[0];
    //PointerWrap p(&ptr, PointerWrap::MODE_READ);
    //getDolphinState(p);

    // Restore preservation blocks
    for (auto it = blocks.begin(); it != blocks.end(); ++it)
    {
        Memory::CopyToEmu(it->address, &preservationMap[*it][0], it->length);
    }
  

}
