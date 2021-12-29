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
#include "Core/Brawlback/Netplay/Netplay.h"

using namespace Brawlback;


typedef std::pair<void*, u32> Buffer;
typedef std::deque<std::unique_ptr<Match::PlayerFrameData>> PlayerFrameDataQueue;


class CEXIBrawlback : public ExpansionInterface::IEXIDevice
{

public:
    CEXIBrawlback();
    ~CEXIBrawlback() override;

    
    void DMAWrite(u32 address, u32 size) override;
    void DMARead(u32 address, u32 size) override;

    bool IsPresent() const;

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
      CMD_TIMESYNC = 16,

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
        CMD_GAME_SETTINGS = 2,
        CMD_FRAME_DATA_ACK = 3,
    };

private:

    // byte vector for sending into to the game
    std::vector<u8> read_queue = {};


    void handleCaptureSavestate(u8* data);
    void handleLoadSavestate(u8* data);
    void handleLocalPadData(u8* data);

    // -------- online stuff -----------

    void handleFindOpponent(u8* payload);
    void handleStartMatch(u8* payload);
    void NetplayThreadFunc();

    void ProcessNetReceive(ENetEvent* event);
    void ProcessRemoteFrameData(Match::PlayerFrameData* framedata);
    void ProcessGameSettings(Match::GameSettings* opponentGameSettings);
    void SendFrameDataToGame(Match::FrameData* framedata);
    void ProcessFrameAck(FrameAck* frameAck);

    ENetHost* server = nullptr;
    std::thread netplay_thread;
    std::unique_ptr<BrawlbackNetplay> netplay;

    bool isHost = true;
    int localPlayerIdx = -1;
    u8 numPlayers = -1;
    bool shouldStall = false;

    // ----------------------------------


    std::unique_ptr<Match::GameSettings> gameSettings;

    int lastFrameAcked[MAX_NUM_PLAYERS] = {0,0,0,0};
    FrameTiming lastFrameTimings[MAX_NUM_PLAYERS] = {};
    std::array<std::deque<FrameTiming>, MAX_NUM_PLAYERS> ackTimers = {};
    u64 pingUs[MAX_NUM_PLAYERS] = {};

    std::deque<std::unique_ptr<BrawlbackSavestate>> savestates = {};
    PlayerFrameDataQueue localPlayerFrameData = {};
    // indexes are player indexes
    std::array<PlayerFrameDataQueue, MAX_NUM_PLAYERS> remotePlayerFrameData = {};
    //               frame, index into PlayerFrameDataQueue
    std::array<std::unordered_map<u32, u32>, MAX_NUM_PLAYERS> remotePlayerFrameDataMap = {};



    std::mutex read_queue_mutex;
    std::mutex remotePadQueueMutex;
    std::mutex ackTimersMutex;

    std::array<bool, MAX_NUM_PLAYERS> hasRemoteInputsThisFrame = {false, false, false, false};


    protected:
    void TransferByte(u8& byte) override;

};
