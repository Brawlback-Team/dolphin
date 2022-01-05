#pragma once

#include "BrawlbackUtility.h"
#include "Core/Brawlback/Netplay/Netplay.h"


// TODO?
// pass numPlayers into ctor or something so we don't have
// to pass it into basically every func here lol

class TimeSync {

public:

    TimeSync();
    ~TimeSync();


    // ---- Funcs that are called by game events

    // called right after sending local inputs over net
    void TimeSyncUpdate(u32 frame, u8 numPlayers);

    // called when we receive remote inputs
    void ReceivedRemoteFramedata(s32 frame, u8 playerIdx, bool hasGameStarted);

    // called when we receive an acknowledgement from opponent of our inputs
    void ProcessFrameAck(FrameAck* frameAck, u8 numPlayers, const std::array<bool, MAX_NUM_PLAYERS>& hasRemoteInputsThisFrame);

    // --------------------------------------------

    // this is the backbone of the time sync logic. Returns whether or not we should stall on *this* current frame.
    bool shouldStallFrame(u32 currentFrame, u32 latestRemoteFrame, u8 numPlayers);

    int getMinAckFrame(u8 numPlayers);

private:

    s32 calcTimeOffsetUs(u8 numPlayers);

    FrameOffsetData frameOffsetData[MAX_NUM_PLAYERS];

    int stallFrameCount = 0;
    bool isConnectionStalled = false;

    bool isSkipping = false;
    int framesToSkip = 0;

    int lastFrameAcked[MAX_NUM_PLAYERS] = {};
    FrameTiming lastFrameTimings[MAX_NUM_PLAYERS] = {};
    std::array<std::deque<FrameTiming>, MAX_NUM_PLAYERS> ackTimers = {};
    u64 pingUs[MAX_NUM_PLAYERS] = {};
    
    std::mutex ackTimersMutex;

};