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

/* NOTES:

Possible todo:
Adapt what slippi does and don't limit the size of input queues by some fixed amount.
Slippi's input queues grow and shrink depending on how much ping there is, and hence, how
many inputs need to be stored.





*/


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


    // DMA handlers
    void handleCaptureSavestate(u8* data);
    void handleLoadSavestate(u8* data);
    void handleLocalPadData(u8* data);
    void handleFindOpponent(u8* payload);
    void handleStartMatch(u8* payload);

    template <typename T>
    void SendCmdToGame(EXICommand cmd, T* payload);

    void SendCmdToGame(EXICommand cmd);
    // -------------------------------


    // --- Net
    void NetplayThreadFunc();
    void ProcessNetReceive(ENetEvent* event);
    void ProcessRemoteFrameData(Match::PlayerFrameData* framedata, u8 numFramedatas);
    void ProcessIndividualRemoteFrameData(Match::PlayerFrameData* framedata);
    void ProcessGameSettings(Match::GameSettings* opponentGameSettings);
    void ProcessFrameAck(FrameAck* frameAck);
    u32 GetLatestRemoteFrame();
    ENetHost* server = nullptr;
    std::thread netplay_thread;
    std::unique_ptr<BrawlbackNetplay> netplay;
    bool isConnected = false;
    // -------------------------------




    // --- Game info
    bool isHost = true;
    int localPlayerIdx = -1;
    u8 numPlayers = -1;
    bool hasGameStarted = false;
    std::unique_ptr<Match::GameSettings> gameSettings;
    // -------------------------------


    // --- Time sync
    void DropAckedInputs(u32 currFrame);
    std::unique_ptr<TimeSync> timeSync;
    // -------------------------------

    
    // --- Rollback
    Match::RollbackInfo rollbackInfo = Match::RollbackInfo();
    void SetupRollback(u32 frame);
    // -------------------------------




    // --- Savestates
    std::deque<std::unique_ptr<BrawlbackSavestate>> savestates = {};
    std::unordered_map<u32, BrawlbackSavestate*> savestatesMap = {};

    std::map<s32, std::unique_ptr<BrawlbackSavestate>> activeSavestates = {};
	std::deque<std::unique_ptr<BrawlbackSavestate>> availableSavestates = {};
    // -------------------------------
    

    // --- Framedata (player inputs)
    void handleSendInputs(u32 frame);
    std::pair<bool, bool> getInputsForGame(Match::FrameData& framedataToSendToGame, u32 frame);
    void storeLocalInputs(Match::PlayerFrameData* localPlayerFramedata);
    PlayerFrameDataQueue localPlayerFrameData = {};
    // indexes are player indexes
    std::array<PlayerFrameDataQueue, MAX_NUM_PLAYERS> remotePlayerFrameData = {};
    // array of players - key is current frame, val is ptr to that frame's (player) framedata
    std::array<std::unordered_map<u32, Match::PlayerFrameData*>, MAX_NUM_PLAYERS> remotePlayerFrameDataMap = {};

    std::array<bool, MAX_NUM_PLAYERS> hasInputsThisFrame = {false, false, false, false}; // tmp - for debugging
    // -------------------------------



    protected:
    void TransferByte(u8& byte) override;

};
