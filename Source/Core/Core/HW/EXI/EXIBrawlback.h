#pragma once
#include "Core/HW/EXI/EXI_Device.h"
#include <string>
#include <array>
#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include <deque>
#include "Core/Brawlback/Savestate.h"
#include "Core/Brawlback/Brawltypes.h"

#define MAX_ROLLBACK_FRAMES 7


class CEXIBrawlback : public ExpansionInterface::IEXIDevice
{

  public:
    CEXIBrawlback();
    ~CEXIBrawlback() override;

    
    void DMAWrite(u32 address, u32 size) override;
    void DMARead(u32 address, u32 size) override;

    bool IsPresent() const;


  private:

    // byte vector for sending into to the game
    std::vector<u8> read_queue = {};

    enum EXICommand
    {
      CMD_UNKNOWN = 0,
      // Online
      CMD_ONLINE_INPUTS = 1,
      CMD_CAPTURE_SAVESTATE = 2,
      CMD_LOAD_SAVESTATE = 3,
      CMD_GET_MATCH_STATE = 4,
      CMD_FIND_OPPONENT = 5,
      CMD_SET_MATCH_SELECTIONS = 6,
      CMD_OPEN_LOGIN = 7,
      CMD_LOGOUT = 8,
      CMD_UPDATE = 9,
      CMD_GET_ONLINE_STATUS = 10,
      CMD_CLEANUP_CONNECTION = 11,
      CMD_SEND_CHAT_MESSAGE = 12,
      CMD_GET_NEW_SEED = 13,
      CMD_REPORT_GAME = 14,
    };


    std::unordered_map<u8, u32> payloadSizes = {
        // The following are used for Slippi online and also have fixed sizes
        {CMD_ONLINE_INPUTS, 17},
        {CMD_CAPTURE_SAVESTATE, 32},
        {CMD_LOAD_SAVESTATE, 32},
        {CMD_GET_MATCH_STATE, 0},
        {CMD_FIND_OPPONENT, 19},
        {CMD_SET_MATCH_SELECTIONS, 8},
        {CMD_SEND_CHAT_MESSAGE, 2},
        {CMD_OPEN_LOGIN, 0},
        {CMD_LOGOUT, 0},
        {CMD_UPDATE, 0},
        {CMD_GET_ONLINE_STATUS, 0},
        {CMD_CLEANUP_CONNECTION, 0},
        {CMD_GET_NEW_SEED, 0},
        {CMD_REPORT_GAME, 16},
    };

    void handleCaptureSavestate(u8* data);
    void handleLoadSavestate(u8* data);


    std::vector<gfPadGamecube> pads = {};

    //std::deque<std::unique_ptr<BrawlbackSavestate>> savestates;
    std::unique_ptr<BrawlbackSavestate> savestateTest;




};
