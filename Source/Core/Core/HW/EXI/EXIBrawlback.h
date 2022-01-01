#pragma once
#include "Core/HW/EXI/EXI_Device.h"
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include "Core/Brawlback/Savestate.h"
#include "Core/Brawlback/BrawlbackUtility.h"
#include "Core/Brawlback/Netplay/Netplay.h"
#include "Core/Brawlback/TimeSync.h"

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

private:

    // byte vector for sending into to the game
    std::vector<u8> read_queue = {};


    // DMAWrite handlers
    void handleCaptureSavestate(u8* data);
    void handleLoadSavestate(u8* data);
    void handleLocalPadData(u8* data);
    void handleFindOpponent(u8* payload);
    void handleStartMatch(u8* payload);
    // -------------------------------


    // --- Net
    void NetplayThreadFunc();
    void ProcessNetReceive(ENetEvent* event);
    void ProcessRemoteFrameData(Match::PlayerFrameData* framedata);
    void ProcessGameSettings(Match::GameSettings* opponentGameSettings);
    void ProcessFrameAck(FrameAck* frameAck);
    u32 GetLatestRemoteFrame();
    ENetHost* server = nullptr;
    std::thread netplay_thread;
    std::unique_ptr<BrawlbackNetplay> netplay;
    // -------------------------------




    // --- Game info
    bool isHost = true;
    int localPlayerIdx = -1;
    u8 numPlayers = -1;
    bool hasGameStarted = false;
    std::unique_ptr<Match::GameSettings> gameSettings;
    // -------------------------------


    // --- Time sync
    void SendTimeSyncToGame();
    void DropAckedInputs(u32 currFrame);
    std::unique_ptr<TimeSync> timeSync;
    // -------------------------------

    
    // --- Rollback
    void SendRollbackCmdToGame(Match::RollbackInfo* rollbackInfo);
    int numFramesWithoutRemoteInputs = 0;
    Match::RollbackInfo rollbackInfo = Match::RollbackInfo();
    // -------------------------------




    // --- Savestates
    std::deque<std::unique_ptr<BrawlbackSavestate>> savestates = {};
    std::unordered_map<u32, u32> savestatesMap = {};
    // -------------------------------
    

    // --- Framedata (player inputs)
    void SendFrameDataToGame(Match::FrameData* framedata);
    PlayerFrameDataQueue localPlayerFrameData = {};
    // indexes are player indexes
    std::array<PlayerFrameDataQueue, MAX_NUM_PLAYERS> remotePlayerFrameData = {};
    // array of players - key is current frame, val is ptr to that frame's (player) framedata
    std::array<std::unordered_map<u32, Match::PlayerFrameData*>, MAX_NUM_PLAYERS> remotePlayerFrameDataMap = {};

    std::array<bool, MAX_NUM_PLAYERS> hasRemoteInputsThisFrame = {false, false, false, false}; // tmp - for debugging
    // -------------------------------


    // --- Mutexes
    std::mutex read_queue_mutex;
    std::mutex remotePadQueueMutex;
    std::mutex localPadQueueMutex;
    std::mutex ackTimersMutex;
    // -------------------------------





    protected:
    void TransferByte(u8& byte) override;

};
