#pragma once

#include "BrawlbackUtility.h"
#include "Core/Brawlback/Netplay/Netplay.h"


class TimeSync {

public:

    TimeSync();
    ~TimeSync();


    void ReceivedRemoteFramedata(u32 frame, u8 playerIdx);
    void ProcessFrameAck(FrameAck* frameack);

private:



};