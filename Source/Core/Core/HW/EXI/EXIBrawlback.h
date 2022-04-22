#pragma once
#include "Core/HW/EXI/EXI_Device.h"
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include "Core/Brawlback/Savestate.h"
#include "Core/Brawlback/BrawlbackUtility.h"
#include "Core/Brawlback/Netplay/Netplay.h"
#include "Core/Brawlback/Netplay/Matchmaking.h"
#include "Core/Brawlback/TimeSync.h"

using namespace Brawlback;

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

    // DMA handlers
    void handleCaptureSavestate(u8* data);
    void handleLoadSavestate(u8* data);
    void handleLocalPadData(u8* data);
    void handleFindMatch(u8* payload);
    void handleStartMatch(u8* payload);

    // REPLAYS
    json curReplay;
    std::vector<u8> curReplaySerialized;
    std::vector<const char*> replays;
    std::vector<const char*> replayNames;
    int curIndex;
    int curFrame;
    void handleNumPlayers(int* payload);
    void handleRandom(u32* payload);
    void handleStage(u8* payload);
    void handleIndex(int* payload);
    void handleStartPosition(float* payload);
    void handleStartFighter(int* payload);
    void handleItemIds(itemIdName* payload, int size);
    void handleItemVarients(u16* payload, int size);
    void handleGame(u32* payload);
    void handleInputs(u8* payload);
    void handlePos(float* payload);
    void handleStick(float* payload);
    void handleActionState(u32* payload);
    void handleStockCount(int* payload);
    void handleFighter(double* payload);
    void handleEndGame();
    void handleGetNumberReplayFiles();
    void handleGetReplayFilesSize();
    void handleGetReplayFilesNamesSize();
    void handleGetReplayFilesNames();
    void handleGetReplayFiles();

    template <typename T>
    void SendCmdToGame(EXICommand cmd, T* payload);

    void SendCmdToGame(EXICommand cmd);
    // -------------------------------


    // --- Net
    void NetplayThreadFunc();
    void ProcessNetReceive(ENetEvent* event);
    void ProcessRemoteFrameData(Match::PlayerFrameDataImpl* framedata, u8 numFramedatas);
    void ProcessIndividualRemoteFrameData(Match::PlayerFrameDataImpl* framedata);
    void ProcessGameSettings(Match::GameSettingsImpl* opponentGameSettings);
    void ProcessFrameAck(FrameAck* frameAck);
    u32 GetLatestRemoteFrame();
    ENetHost* server = nullptr;
    std::thread netplay_thread;
    std::unique_ptr<BrawlbackNetplay> netplay;

    bool isConnected = false;
    // -------------------------------



    // --- Matchmaking
    void connectToOpponent();
    void MatchmakingThreadFunc();
    Brawlback::UserInfo getUserInfo();
    Matchmaking::MatchSearchSettings lastSearch;
    std::unique_ptr<Matchmaking> matchmaking;
    std::thread matchmaking_thread;
    // -------------------------------




    // --- Game info
    bool isHost = true;
    int localPlayerIdx = -1;
    u8 numPlayers = 0;
    bool hasGameStarted = false;
    std::unique_ptr<Match::GameSettingsImpl> gameSettings;
    // -------------------------------


    // --- Time sync
    std::unique_ptr<TimeSync> timeSync;
    // -------------------------------

    
    // --- Rollback
    Match::RollbackInfoImpl rollbackInfo = Match::RollbackInfoImpl();
    void SetupRollback(u32 frame);
    void HandleLocalInputsDuringPrediction(u32 frame, u8 playerIdx);
    // -------------------------------



    // --- Savestates
    std::deque<std::unique_ptr<BrawlbackSavestate>> savestates = {};
    std::unordered_map<u32, BrawlbackSavestate*> savestatesMap = {};

    std::map<s32, std::unique_ptr<BrawlbackSavestate>> activeSavestates = {};
	std::deque<std::unique_ptr<BrawlbackSavestate>> availableSavestates = {};
    // -------------------------------
    

    // --- Framedata (player inputs)
    void handleSendInputs(u32 frame);
    std::pair<bool, bool> getInputsForGame(Match::FrameDataImpl& framedataToSendToGame, u32 frame);
    void storeLocalInputs(Match::PlayerFrameDataImpl* localPlayerFramedata);

    // local player input history. Always holds FRAMEDATA_MAX_QUEUE_SIZE of past inputs
    PlayerFrameDataQueue localPlayerFrameData = {};

    // remote player input history (indexes are player indexes). Always holds FRAMEDATA_MAX_QUEUE_SIZE of past inputs
    std::array<PlayerFrameDataQueue, MAX_NUM_PLAYERS> remotePlayerFrameData = {};
    // array of players - key is current frame, val is ptr to that frame's (player)framedata
    std::array<std::unordered_map<u32, Match::PlayerFrameDataImpl*>, MAX_NUM_PLAYERS> remotePlayerFrameDataMap = {};
    // -------------------------------



    protected:
    void TransferByte(u8& byte) override;

};
