#pragma once

#include "SlippiUtility.h"

// https://docs.google.com/spreadsheets/d/1xVvcsGZg930uVhIawacDp-brbNpLJQtzj3ry-ZQaXWo/edit?usp=sharing

namespace MemRegions
{

static std::vector<PreserveBlock> excludeSections = {
    // {start address, size}

    {0x935D0000, 0x935E0000 - 0x935D0000},  // CPP Framework heap (subject to change...??)
    // might also need the initializer sections(?) where global statics are stored
    // cpp framework code sections
    {0x817da5a4, 0x81FFFFFF - 0x817da5a4},
    {0x8055A600, 0x80563100 - 0x8055A600},
    {0x805B5200, 0x805B61D0 - 0x805B5200},
    {0x817CE880, 0x817DA590 - 0x817CE880},

    {0x80663e00, 0x1a4},  // CameraController
    {0x80663b40, 0x198},  // cmAiController
    {0x805b6d20, 0x740},  // gfCameraManager

};

static std::vector<ssBackupLoc> memRegions = {
    // {start address, end address, nullptr, "NameOfSection"},

    // ============================= mem1 =============================

    {0x805b8a00, 0x805b8a00 + 0x17c, nullptr, "gfTaskScheduler"},  // gfTaskScheduler

    {0x80611f60, 0x80673460, nullptr, "System"},             // System
    {0x80b8db60, 0x80c23a60, nullptr, "Effect"},             // Effect
    {0x8123ab60, 0x8128cb60, nullptr, "Fighter1Instance"},   // Fighter1Instance
    {0x8128cb60, 0x812deb60, nullptr, "Fighter2Instance"},   // Fighter2Instance
    {0x81601960, 0x81734d60, nullptr, "InfoInstance"},       // InfoInstance
    {0x815edf60, 0x817bad60, nullptr, "InfoExtraResource"},  // InfoExtraResource
    {0x80c23a60, 0x80da3a60, nullptr, "InfoResource"},       // InfoResource
    {0x80da3a60, 0x80fd6260, nullptr, "CommonResource"},     // CommonResource
    {0x81049e60, 0x81061060, nullptr, "Tmp"},                // Tmp
    {0x8154e560, 0x81601960, nullptr, "Physics"},            // Physics

    //{0x814ce460, 0x8154e560, nullptr, "StageInstance"}, // StageInstance
    //{0x81734d60, 0x817ce860, nullptr, "MenuInstance"}, // MenuInstance

    //{0x80673460, 0x80b8db60, nullptr, "OverlayCommon"}, // OverlayCommon
    {0x80A471A0, 0x80b8db60, nullptr, "OverlayCommon 4/4"},  // OverlayCommon 4/4

    //{0x81061060, 0x810a9560, nullptr, "OverlayFighter1"}, // OverlayFighter1
    //{0x810a9560, 0x810f1a60, nullptr, "OverlayFighter2"}, // OverlayFighter2

    // ============================= mem2 =============================

    {0x90e61400, 0x90e77500, nullptr, "WiiPad"},  // WiiPad

    {0x9151fa00, 0x91a72e00, nullptr, "Fighter1Resource"},
    //{0x9151fa00, 0x917C9400, nullptr, "first half of Fighter1Resource"}, // first half of
    //Fighter1Resource
    {0x91a72e00, 0x91b04c80, nullptr, "Fighter1Resource2"},  // Fighter1Resource2

    {0x91b04c80, 0x92058080, nullptr, "Fighter2Resource"},
    //{0x91b04c80, 0x91DAE680, nullptr, "Fighter2Resource first half"}, // Fighter2Resource first
    //half
    {0x92058080, 0x920e9f00, nullptr, "Fighter2Resource2"},  // Fighter2Resource2

    {0x91478e00, 0x914d2900, nullptr, "FighterEffect"},  // FighterEffect

    {0x92cb4400, 0x92dcdf00, nullptr, "FighterTechqniq"},  // FighterTechqniq

    {0x9134cc00, 0x9134cc10, nullptr, "CopyFB_Edited"},
    //{0x9134cc00, 0x91478e00, nullptr, "CopyFB"}, // CopyFB

    {0x90167400, 0x90199800, nullptr, "GameGlobal"},  // GameGlobal

    {0x90ff5120, 0x90ff5130, nullptr,
     "GlobalMode1"},  // two GlobalMode addrs that apparently change sometimes
    {0x90ff5150, 0x90ff51c0, nullptr, "GlobalMode2"},

};

}  // namespace MemRegions
