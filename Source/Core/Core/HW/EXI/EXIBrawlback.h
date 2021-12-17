#pragma once
#include "Core/HW/EXI/EXI_Device.h"
#include <string>
#include <vector>
//#include <iostream>
//#include <fstream>
#include <memory>
#include <deque>
#include "Core/Brawlback/Savestate.h"
#include "Core/Brawlback/BrawlbackUtility.h"
#include "Core/Brawlback/SlippiNetplay/SlippiNetplay.h"

using namespace Brawlback;

#define MAX_ROLLBACK_FRAMES 7
#define MAX_REMOTE_PLAYERS 3
#define BRAWLBACK_PORT 7779


typedef std::pair<void*, u32> Buffer;


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

    enum EXICommand : u8
    {
      CMD_UNKNOWN = 0,

      // Online

      CMD_ONLINE_INPUTS = 1,
      CMD_CAPTURE_SAVESTATE = 2,
      CMD_LOAD_SAVESTATE = 3,

      CMD_FIND_OPPONENT = 5,
      CMD_START_MATCH = 13,
      CMD_SETUP_PLAYERS = 14,
      CMD_FRAMEDATA = 15,

      CMD_GET_MATCH_STATE = 4,
      CMD_SET_MATCH_SELECTIONS = 6,

      CMD_OPEN_LOGIN = 7,
      CMD_LOGOUT = 8,
      CMD_UPDATE = 9,
      
      CMD_GET_ONLINE_STATUS = 10,
      CMD_CLEANUP_CONNECTION = 11,
      CMD_GET_NEW_SEED = 12,
    };

    enum NetPacketCommand : u8 
    {
        CMD_FRAME_DATA = 1,
    };


    std::unordered_map<u8, u32> payloadSizes = {
        // The following are used for Slippi online and also have fixed sizes
        {CMD_ONLINE_INPUTS, 17},
        {CMD_CAPTURE_SAVESTATE, 32},
        {CMD_LOAD_SAVESTATE, 32},
        {CMD_GET_MATCH_STATE, 0},
        {CMD_FIND_OPPONENT, 19},
        {CMD_SET_MATCH_SELECTIONS, 8},
        {CMD_OPEN_LOGIN, 0},
        {CMD_LOGOUT, 0},
        {CMD_UPDATE, 0},
        {CMD_GET_ONLINE_STATUS, 0},
        {CMD_CLEANUP_CONNECTION, 0},
        {CMD_GET_NEW_SEED, 0},
    };

    void handleCaptureSavestate(u8* data);
    void handleLoadSavestate(u8* data);
    void handlePadData(u8* data);

    // -------- online stuff -----------

    void handleFindOpponent(u8* payload);
    void handleStartMatch(u8* payload);
    void NetplayThreadFunc();

    void ProcessNetPacket(ENetPacket* pckt);
    void ProcessRemoteFrameData(Match::FrameData* framedata);

    ENetHost* server = nullptr;
    std::thread netplay_thread;

    bool isHost = true;

    // ----------------------------------






    std::deque<std::unique_ptr<BrawlbackSavestate>> savestates;
    std::deque<std::unique_ptr<Match::FrameData>> playersFrameData;



    std::mutex read_queue_mutex;


    protected:
    void TransferByte(u8& byte) override;

};
