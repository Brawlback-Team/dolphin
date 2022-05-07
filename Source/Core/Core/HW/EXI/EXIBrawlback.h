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

  std::string replayDirectory;

private:

    // byte vector for sending into to the game
    std::vector<u8> read_queue = {};

  // --- DMA handlers
  void handleCaptureSavestate(u8* data);
  void handleLoadSavestate(u8* data);
  void handleLocalPadData(u8* data);
  void handleFindMatch(u8* payload);
  void handleStartMatch(u8* payload);
  void handleStartReplaysStruct(u8* payload);
  void serializeStartReplay(const StartReplay& startReplay);
  void serializeReplay(const Replay& replay);
  void handleReplaysStruct(u8* payload);
  void handleEndOfReplay();
  void handleGetNextFrame(u8* payload, int index);
  void handleNumReplays();
  void handleGetStartReplay(u8* payload);

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

  // --- Replays
  void fixStartReplayEndianness(StartReplay& startReplay);
  void fixReplayEndianness(Replay& replay);
  std::vector<std::vector<u8>> getReplays(std::string path);
  std::vector<std::string> getReplayNames(std::string path);
  u8 getNumReplays(std::string path);
  json getReplayJsonAtIndex(int index);
  std::string getReplayNameAtIndex(int index);
  u8 curIndex;
  json curReplayJson;
  std::string curReplayName;
  // -------------------------------

    // --- Net
    void MatchmakingThreadFunc();
    void NetplayThreadFunc();
    void ProcessNetReceive(ENetEvent* event);
    void ProcessRemoteFrameData(PlayerFrameData* framedata, u8 numFramedatas);
    void ProcessIndividualRemoteFrameData(PlayerFrameData* framedata);
    void ProcessGameSettings(GameSettings* opponentGameSettings);
    void ProcessFrameAck(FrameAck* frameAck);
    u32 GetLatestRemoteFrame();
    ENetHost* server = nullptr;
    Matchmaking::MatchSearchSettings lastSearch;
    std::thread netplay_thread;
    std::thread matchmaking_thread;
    std::unique_ptr<BrawlbackNetplay> netplay;
    std::unique_ptr<Matchmaking> matchmaking;

    bool isConnected = false;
    // -------------------------------




    // --- Game info
    bool isHost = true;
    int localPlayerIdx = -1;
    u8 numPlayers = -1;
    bool hasGameStarted = false;
    std::unique_ptr<GameSettings> gameSettings;
    // -------------------------------

    Brawlback::UserInfo getUserInfo();

    // --- Time sync
    void DropAckedInputs(u32 currFrame);
    std::unique_ptr<TimeSync> timeSync;
    // -------------------------------

    
    // --- Rollback
    RollbackInfo rollbackInfo = RollbackInfo();
    void SetupRollback(u32 frame);
    void HandleLocalInputsDuringPrediction(u32 frame, u8 playerIdx);
    // -------------------------------

    void connectToOpponent();


    // --- Savestates
    std::deque<std::unique_ptr<BrawlbackSavestate>> savestates = {};
    std::unordered_map<u32, BrawlbackSavestate*> savestatesMap = {};

    std::map<s32, std::unique_ptr<BrawlbackSavestate>> activeSavestates = {};
	std::deque<std::unique_ptr<BrawlbackSavestate>> availableSavestates = {};
    // -------------------------------
    

    // --- Framedata (player inputs)
    void handleSendInputs(u32 frame);
    std::pair<bool, bool> getInputsForGame(FrameData& framedataToSendToGame, u32 frame);
    void storeLocalInputs(PlayerFrameData* localPlayerFramedata);

    // local player input history
    PlayerFrameDataQueue localPlayerFrameData = {};

    //std::unordered_map<u32, Match::PlayerFrameData*> localPlayerFrameDataMap = {};

    // remote player input history (indexes are player indexes)
    std::array<PlayerFrameDataQueue, MAX_NUM_PLAYERS> remotePlayerFrameData = {};
    // array of players - key is current frame, val is ptr to that frame's (player)framedata
    std::array<std::unordered_map<u32, PlayerFrameData*>, MAX_NUM_PLAYERS> remotePlayerFrameDataMap = {};
    // -------------------------------



    protected:
    void TransferByte(u8& byte) override;

};
