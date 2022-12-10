#pragma once
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "Core/Brawlback/BrawlbackUtility.h"
#include "Core/Brawlback/Netplay/Matchmaking.h"
#include "Core/Brawlback/Netplay/Netplay.h"
#include "Core/Brawlback/Savestate.h"
#include "Core/Brawlback/TimeSync.h"
#include "Core/HW/EXI/EXI_Device.h"

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
  void handleFrameDataRequest(u8* data);
  void handleFrameAdvanceRequest(u8* data);
  void handleFindMatch(u8* payload);
  void handleStartMatch(u8* payload);
  void handleEndMatch(u8* payload);
  void handleStartReplaysStruct(u8* payload);
  void serializeStartReplay(const StartReplay& startReplay);
  void serializeReplay(const Replay& replay);
  void handleReplaysStruct(u8* payload);
  void handleEndOfReplay();
  void handleGetNextFrame(u8* payload, int index);
  void handleNumReplays();
  void handleGetStartReplay(u8* payload);


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
  void NetplayThreadFunc();
  void ProcessNetReceive(ENetEvent* event);
  void ProcessRemoteFrameData(PlayerFrameData* framedata, u8 numFramedatas);
  void ProcessIndividualRemoteFrameData(PlayerFrameData* framedata);
  void ProcessGameSettings(GameSettings* opponentGameSettings);
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
  GameSettings gameSettings;
  int gameIndex = 0;
  // -------------------------------

  // --- Time sync
  std::unique_ptr<TimeSync> timeSync;
  // -------------------------------

  // --- Rollback
  bool isPredicting;          // if we are using past inputs for this frame or not
  FrameData predictedInputs;  // predicted inputs from some previous frame
  u32 framesToAdvance = 1;    // number of "frames" to advance the simulation on this frame
  int latestConfirmedFrame =
      0;  // Tracks the last frame where we synchronized the game state with the remote client

  void updateSync(s32& localFrame, u8 playerIdx);
  bool shouldRollback(s32 localFrame);
  void LoadState(s32 rollbackFrame);
  void SaveState(s32 frame);

  // -------------------------------

  // --- Savestates
  std::deque<std::unique_ptr<BrawlbackSavestate>> savestates = {};
  std::unordered_map<u32, BrawlbackSavestate*> savestatesMap = {};

  std::map<s32, std::unique_ptr<BrawlbackSavestate>> activeSavestates = {};
  std::deque<std::unique_ptr<BrawlbackSavestate>> availableSavestates = {};
  // -------------------------------

  // --- Framedata (player inputs)
  void handleSendInputs(u32 frame);
  PlayerFrameData getLocalInputs(const s32& frame);
  PlayerFrameData getRemoteInputs(s32& frame, u8 playerIdx);
  void storeLocalInputs(PlayerFrameData* localPlayerFramedata);

  // local player input history. Always holds FRAMEDATA_MAX_QUEUE_SIZE of past inputs
  PlayerFrameDataQueue localPlayerFrameData = {};

  // remote player input history (indexes are player indexes). Always holds FRAMEDATA_MAX_QUEUE_SIZE
  // of past inputs
  std::array<PlayerFrameDataQueue, MAX_NUM_PLAYERS> remotePlayerFrameData = {};
  // -------------------------------

protected:
  void TransferByte(u8& byte) override;
};
