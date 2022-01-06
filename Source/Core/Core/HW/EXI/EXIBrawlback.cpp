

#include "EXIBrawlback.h"

#include "Core/HW/Memmap.h"
#include <chrono>



CEXIBrawlback::CEXIBrawlback()
{
    INFO_LOG(BRAWLBACK, "BRAWLBACK exi ctor");
    auto enet_init_res = enet_initialize();
    if (enet_init_res < 0) {
        ERROR_LOG(BRAWLBACK, "Failed to init enet! %d\n", enet_init_res);
    }
    else if (enet_init_res == 0) {
        INFO_LOG(BRAWLBACK, "Enet init success");
    }
    this->netplay = std::make_unique<BrawlbackNetplay>();
    this->timeSync = std::make_unique<TimeSync>();
}

CEXIBrawlback::~CEXIBrawlback()
{
    enet_deinitialize();
    enet_host_destroy(this->server);
    if (this->netplay_thread.joinable()) {
        this->netplay_thread.join();
    }
}




void CEXIBrawlback::handleCaptureSavestate(u8* data)
{
    int idx = 0;
    u32 frame = SlippiUtility::Mem::readWord(data, idx, 999, 0);

    std::unique_ptr<BrawlbackSavestate> ss = std::make_unique<BrawlbackSavestate>(frame); // ss = savestate

    if (this->savestates.size() + 1 > MAX_ROLLBACK_FRAMES)
    {
        BrawlbackSavestate* front_ss = this->savestates.front().release();
        this->savestatesMap.erase(front_ss->frame);
        delete front_ss;
        this->savestates.pop_front(); // pop savestate from front of queue if we would go over the max # of rollback frames
    }


    auto start = std::chrono::high_resolution_clock::now();
    ss->Capture();
    auto finish = std::chrono::high_resolution_clock::now();


    std::chrono::duration<double> elapsed = finish - start;
    INFO_LOG(BRAWLBACK, "Capture Savestate for frame %u took %f\n", frame, elapsed.count());

    this->savestates.push_back(std::move(ss));
    this->savestatesMap[frame] = this->savestates.back().get();
}

void CEXIBrawlback::handleLoadSavestate(u8* data)
{

    Match::RollbackInfo* loadStateRollbackInfo = (Match::RollbackInfo*)data;
    // the frame we should load state for is the frame we first began not receiving inputs
    u32 loadStateFrame = Common::swap32(loadStateRollbackInfo->beginFrame);

    if (!savestatesMap.count(loadStateFrame)) {
        INFO_LOG(BRAWLBACK, "Couldn't find frame %u in savestate map!\n", loadStateFrame);
        return;
    }

    BrawlbackSavestate* savestate = savestatesMap[loadStateFrame];

    // Fetch preservation blocks
    std::vector<PreserveBlock> blocks = {};

    if (loadStateRollbackInfo->hasPreserveBlocks) // populate preservation blocks
    {
        // first 4 bytes are game frame
        //s32 frame = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
        //INFO_LOG(BRAWLBACK, "Loading savestate for frame: %u\n", frame);
        // rest of data is preservation blocks
        u32* preserveArr = (u32*)&loadStateRollbackInfo->preserveBlocks;

        int idx = 0;
        while (Common::swap32(preserveArr[idx]) != 0)
        {
            // each PreserveBlock is made up of 8 bytes. 4 for address and 4 for length
            PreserveBlock p = {Common::swap32(preserveArr[idx]), Common::swap32(preserveArr[idx + 1])};
            blocks.push_back(p);
            idx += 2;  // increment by 8 bytes
        }
    }
    else {
        INFO_LOG(BRAWLBACK, "no preservation blocks\n");
    }

    if (savestate)
    {
        INFO_LOG(BRAWLBACK, "Loading state for frame %u\n", loadStateFrame);
        auto start = std::chrono::high_resolution_clock::now();
        savestate->Load(blocks);
        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - start;
        INFO_LOG(BRAWLBACK, "Load Savestate took %f\n", elapsed.count());
    }
 
}

template <typename T>
void CEXIBrawlback::SendCmdToGame(EXICommand cmd, T* payload) {
    std::lock_guard<std::mutex> lock (this->read_queue_mutex);
    this->read_queue.clear();
    this->read_queue.push_back(cmd);
    if (payload) {
        std::vector<u8> byteVec = Mem::structToByteVector(payload);
        this->read_queue.insert(this->read_queue.end(), byteVec.begin(), byteVec.end());
    }
}

void CEXIBrawlback::SendCmdToGame(EXICommand cmd) {
    std::lock_guard<std::mutex> lock (this->read_queue_mutex);
    this->read_queue.clear();
    this->read_queue.push_back(cmd);
}

Match::PlayerFrameData CreateDummyPlayerFrameData(u32 frame, u8 playerIdx) {
    Match::PlayerFrameData dummy_framedata = Match::PlayerFrameData();
    dummy_framedata.frame = frame;
    dummy_framedata.playerIdx = playerIdx;
    dummy_framedata.pad = gfPadGamecube(); // empty pad
    return dummy_framedata;
}

// `data` is a ptr to a PlayerFrameData struct
// this is called every frame at the beginning of the frame
void CEXIBrawlback::handleLocalPadData(u8* data)
{
    Match::PlayerFrameData* playerFramedata = (Match::PlayerFrameData*)data;
    int idx = 0;
    // first 4 bytes are current game frame
    u32 frame = SlippiUtility::Mem::readWord(data, idx, 999, 0); // properly switched endianness
    //u8 playerIdx = playerFramedata->playerIdx;
    playerFramedata->frame = frame; // properly switched endianness

    if (frame == GAME_START_FRAME) {
        this->hasGameStarted = true;
        this->rollbackInfo = Match::RollbackInfo();
    }

    #if ROLLBACK_IMPL
    if (this->rollbackInfo.pastFrameDataPopulated) {
        INFO_LOG(BRAWLBACK, "Past frame data is populated! Sending rollback cmd to game\n");
        this->rollbackInfo.isUsingPredictedInputs = false;
        this->SendCmdToGame(EXICommand::CMD_ROLLBACK, &this->rollbackInfo);
        this->rollbackInfo = Match::RollbackInfo(); // reset rollbackInfo
        return;
    }
    #endif

    bool shouldTimeSync = this->timeSync->shouldStallFrame(frame, this->GetLatestRemoteFrame(), this->numPlayers);
    if (shouldTimeSync) {
        INFO_LOG(BRAWLBACK, "Sending time sync command for this frame\n");
        // Send inputs that have not yet been acked
        this->handleSendInputs(frame);
        this->SendCmdToGame(EXICommand::CMD_TIMESYNC);
        return;
    }


    // store these local inputs (with frame delay)
    this->storeLocalInputs(playerFramedata, frame);
    // broadcasts local inputs (with frame delay)
    this->handleSendInputs(frame);

    // getting inputs for all players & sending them to game
    Match::FrameData framedataToSendToGame = Match::FrameData();
    framedataToSendToGame.randomSeed = 0x496ffd00; // tmp
    // populates playerFrameDatas field of the above FrameData
    std::pair<bool, bool> foundData = this->getInputsForGame(framedataToSendToGame, frame);

    if (!foundData.second && frame > FRAME_DELAY) { // if we didn't find remote inputs
        INFO_LOG(BRAWLBACK, "Sending time sync command for this frame\n");
        this->SendCmdToGame(EXICommand::CMD_TIMESYNC);
    }
    else {
        this->SendCmdToGame(EXICommand::CMD_FRAMEDATA, &framedataToSendToGame);
    }



}

void CEXIBrawlback::storeLocalInputs(Match::PlayerFrameData* localPlayerFramedata, u32 frame) {
    std::lock_guard<std::mutex> local_lock (this->localPadQueueMutex);
    std::unique_ptr<Match::PlayerFrameData> pFD = std::make_unique<Match::PlayerFrameData>(*localPlayerFramedata);
    // local inputs offset by FRAME_DELAY to mask latency
    // Once we hit frame X, we send inputs for that frame, but pretend they're from frame X+2
    // so those inputs now have an extra 2 frames to get to the opponent before the opponent's
    // client hits frame X+2. 
    pFD->frame = frame + FRAME_DELAY;
    INFO_LOG(BRAWLBACK, "Frame %u PlayerIdx: %u numPlayers %u\n", frame, localPlayerFramedata->playerIdx, this->numPlayers);
    
    // don't care about storing this framedata if it's a frame we've already stored.
    // this addresses the issue with ping spikes and time syncing during them. When the game stalls, 
    // it'll be on one frame for a while, and if local inputs continue to be pushed onto the queue,
    // with high enough ping, eventually local inputs that we still need for the next frames will be popped off
    if (!this->localPlayerFrameData.empty() && pFD->frame <= this->localPlayerFrameData.back()->frame) {
        INFO_LOG(BRAWLBACK, "Didn't push local framedata for frame %u\n", pFD->frame);
        return;
    }

    // store local framedata
    if (this->localPlayerFrameData.size() + 1 > FRAMEDATA_MAX_QUEUE_SIZE) {
        //WARN_LOG(BRAWLBACK, "Hit local player framedata queue max size! %u\n", this->localPlayerFrameData.size());
        Match::PlayerFrameData* pop_data = this->localPlayerFrameData.front().release();
        INFO_LOG(BRAWLBACK, "Popping local framedata for frame %u\n", pop_data->frame);
        free(pop_data);
        this->localPlayerFrameData.pop_front();
    }
    this->localPlayerFrameData.push_back(std::move(pFD));
}

void CEXIBrawlback::handleSendInputs(u32 frame) {
    // broadcast this local framedata
    if (!this->localPlayerFrameData.empty()) {
        std::lock_guard<std::mutex> local_lock (this->localPadQueueMutex);

        // this strat is taken from slippi [ thanks fizzi <3 ]
        // each frame we send local inputs to the other client(s)
        // those clients then acknowledge those inputs and send that ack(nowledgement)
        // back to us. All the inputs that are acked shouldn't be kept track of in the
        // local pad queue since we know for a fact the remote client has them/
        // Inputs that *are* kept track of in the local pad queue are inputs that
        // we don't know for sure the remote client has received.
        // that's why we send *all* local inputs with every packet.
        // so that when the remote client doesn't receive inputs, and needs to rollback
        // the next packet will have all the inputs that that client didn't receive.
        this->DropAckedInputs(frame);

        int localPadQueueSize = (int)this->localPlayerFrameData.size();
        INFO_LOG(BRAWLBACK, "Local pad queue size: %u\n", localPadQueueSize);
        if (localPadQueueSize == 0) return; // if no inputs, nothing to send

        std::vector<Match::PlayerFrameData*> localFramedatas = {};
        // push framedatas from back to front
        // this means the resulting vector (localFramedatas) will have the most
        // recent framedata in the first position, and the oldest framedata in the last position
        for (int i = localPadQueueSize-1; i >= 0; i--) {
            const auto& localFramedata = this->localPlayerFrameData[i];
            if (localFramedatas.empty() || ( !localFramedatas.empty() && localFramedatas.back()->frame > localFramedata->frame) ) {
                localFramedatas.push_back(localFramedata.get());
            }
        }

        INFO_LOG(BRAWLBACK, "Broadcasting %i framedatas\n", localFramedatas.size());
        this->netplay->BroadcastPlayerFrameDataWithPastFrames(this->server, localFramedatas);
        u32 mostRecentFrame = this->localPlayerFrameData.back()->frame; // with delay
        this->timeSync->TimeSyncUpdate(mostRecentFrame, this->numPlayers);

    }
}

std::pair<bool, bool> CEXIBrawlback::getInputsForGame(Match::FrameData& framedataToSendToGame, u32 frame) {
    // TODO (pine):
    // clean up this mess

    std::lock_guard<std::mutex> lock (this->remotePadQueueMutex);

    // first is if we've found local inputs, second is if we've found remote inputs
    std::pair<bool, bool> foundData = std::make_pair(false, false);

    // for each remote player
    for (int playerIdx = 0; playerIdx < this->numPlayers; playerIdx++) {
        //bool foundData = false;
        // search for local player's inputs
        if (playerIdx == this->localPlayerIdx && !this->localPlayerFrameData.empty()) {
            std::lock_guard<std::mutex> local_lock (this->localPadQueueMutex);
            for (int i = 0; i < this->localPlayerFrameData.size(); i++) {
                // find local framedata for this frame
                if (this->localPlayerFrameData[i]->frame == frame) {
                    INFO_LOG(BRAWLBACK, "found local inputs\n");
                    framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] = *(this->localPlayerFrameData[i].get());
                    foundData.first = true;
                    break;
                }
            }
            if (!foundData.first) {
                ERROR_LOG(BRAWLBACK, "Couldn't find local inputs! Using empty pad.\n");
                framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] = CreateDummyPlayerFrameData(frame, this->localPlayerIdx);
            }
            continue;
        }
        // search for remote player's inputs
        if (!this->remotePlayerFrameData.empty() && !this->remotePlayerFrameData[playerIdx].empty()) {
            PlayerFrameDataQueue& remotePlayerFrameDataQueue = this->remotePlayerFrameData[playerIdx];
            // find framedata in queue that has the frame we want to inject into the game (current frame - frame delay)
            //INFO_LOG(BRAWLBACK, "Remote framedata q range: %u - %u\n", remotePlayerFrameDataQueue.front()->frame, remotePlayerFrameDataQueue.back()->frame);
            for (int i = 0; i < remotePlayerFrameDataQueue.size(); i++) {
                if (remotePlayerFrameDataQueue[i]) {
                    Match::PlayerFrameData* framedata = remotePlayerFrameDataQueue[i].get();
                    // find remote framedata for this frame
                    if (framedata->frame == frame) {
                        INFO_LOG(BRAWLBACK, "found remote inputs\n");
                        hasRemoteInputsThisFrame[playerIdx] = true;
                        framedataToSendToGame.playerFrameDatas[playerIdx] = *framedata;
                        foundData.second = true;
                        break;
                    }
                }
            }
        }
        
        #if ROLLBACK_IMPL
        if (!foundData.second) { // didn't find framedata for this frame.
            INFO_LOG(BRAWLBACK, "no remote framedata - frame %u remotePIdx %i\n", frame, playerIdx);
            hasRemoteInputsThisFrame[playerIdx] = false;
            if (this->remotePlayerFrameData[playerIdx].size() >= MAX_ROLLBACK_FRAMES) {

                if (!this->rollbackInfo.isUsingPredictedInputs) {
                    INFO_LOG(BRAWLBACK, "Trying to find frame for predicted inputs...\n");

                    // maybe use frameWithDelay?
                    u32 searchEndFrame = frame >= MAX_ROLLBACK_FRAMES ? frame - MAX_ROLLBACK_FRAMES : 0; // clamp to 0
                    // iterate MAX_ROLLBACK_FRAMES into the past to find player framedata
                    // this is where we """"predict""""" player inputs when we don't receive them.
                    for (u32 frameIter = frame; frameIter > searchEndFrame; frameIter--) { 
                        // find most recent frame that exists
                        if (this->remotePlayerFrameDataMap[playerIdx].count(frameIter)) {
                            INFO_LOG(BRAWLBACK, "found frame for predicting inputs %u\n", frameIter);
                            // get index into framedata queue from map
                            //int mostRecentFramedataIdx = this->remotePlayerFrameDataMap[playerIdx][frameIter];
                            // get PlayerFrameData from queue using idx we just got
                            Match::PlayerFrameData* mostRecentFramedata = this->remotePlayerFrameDataMap[playerIdx][frameIter];
                            // copy it into the framedata that'll be sent to the game
                            framedataToSendToGame.playerFrameDatas[playerIdx] = *mostRecentFramedata;

                            foundData.second = true;

                            // set rollback info
                            this->rollbackInfo.isUsingPredictedInputs = true;
                            this->rollbackInfo.beginFrame = frameIter;
                            this->rollbackInfo.predictedInputs = *mostRecentFramedata;
                            break;
                        }
                    }

                    if (!foundData.second) {
                        // couldn't find relevant past framedata
                        // this probably means the difference between clients is greater than MAX_ROLLBACK_FRAMES
                        // we should probably do a time-sync here.

                        WARN_LOG(BRAWLBACK, "Couldn't find framedata when we should rollback!! Sending empty framedata...\n");
                        //this->SendCmdToGame(EXICommand::CMD_TIMESYNC); // maybe do this?
                        //return;
                        framedataToSendToGame.playerFrameDatas[playerIdx] = CreateDummyPlayerFrameData(frame, playerIdx);
                    }
                }

                else {
                    // we've already encountered a frame without inputs, and have set rollbackinfo, so just use those predicted inputs
                    INFO_LOG(BRAWLBACK, "Using predicted inputs from frame %u\n", this->rollbackInfo.predictedInputs.frame);
                    framedataToSendToGame.playerFrameDatas[playerIdx] = this->rollbackInfo.predictedInputs;
                    this->numFramesWithoutRemoteInputs += 1;

                    if (this->numFramesWithoutRemoteInputs > MAX_ROLLBACK_FRAMES) {
                        INFO_LOG(BRAWLBACK, "Num frames without remote inputs exceedes max rollback frames\n");
                        //if (frame > 80) {
                        //    this->SendCmdToGame(EXICommand::CMD_TIMESYNC);
                        //    return;
                        //}
                    }
                }

            }
            else {
                ERROR_LOG(BRAWLBACK, "Too early of a frame. Can't rollback. Sending dummy pad\n");
                framedataToSendToGame.playerFrameDatas[playerIdx] = CreateDummyPlayerFrameData(frame, playerIdx);
            }
        }
        #else
        if (!foundData.second) { // didn't find framedata for this frame.
            INFO_LOG(BRAWLBACK, "no remote framedata - frame %u remotePIdx %i\n", frame, playerIdx);
            hasRemoteInputsThisFrame[playerIdx] = false;
            framedataToSendToGame.playerFrameDatas[playerIdx] = CreateDummyPlayerFrameData(frame, playerIdx);
        }
        #endif
    }

    return foundData;
}

void CEXIBrawlback::DropAckedInputs(u32 currFrame) {
    // Remove pad reports that have been received and acked
    u32 minAckFrame = (u32)this->timeSync->getMinAckFrame(this->numPlayers);
    minAckFrame = minAckFrame > currFrame ? currFrame : minAckFrame; // clamp to current frame to prevent it dropping local inputs that haven't been used yet
    //INFO_LOG(BRAWLBACK, "Checking to drop local inputs, oldest frame: %d | minAckFrame: %u",
    //            this->localPlayerFrameData.front()->frame, minAckFrame);
    //INFO_LOG(BRAWLBACK, "Local input queue frame range: %u - %u\n", this->localPlayerFrameData.front()->frame, this->localPlayerFrameData.back()->frame);
    while (!this->localPlayerFrameData.empty() && this->localPlayerFrameData.front()->frame < minAckFrame)
    {
        //INFO_LOG(BRAWLBACK, "Dropping local input for frame %d from queue", this->localPlayerFrameData.front()->frame);
        this->localPlayerFrameData.pop_front();
    }
}

u32 CEXIBrawlback::GetLatestRemoteFrame() {
    u32 lowestFrame = 0;
	for (int i = 0; i < this->numPlayers; i++)
	{
        if (i == this->localPlayerIdx) continue;

		if (this->remotePlayerFrameData[i].empty())
		{
			return 0;
		}

		u32 f = this->remotePlayerFrameData[i].back()->frame;
		if (f < lowestFrame || lowestFrame == 0)
		{
			lowestFrame = f;
		}
	}

	return lowestFrame;
}

void BroadcastFramedataAck(Match::PlayerFrameData* framedata, BrawlbackNetplay* netplay, ENetHost* server) {
    FrameAck ackData;
    ackData.frame = (int)framedata->frame;
    ackData.playerIdx = framedata->playerIdx;
    sf::Packet ackDataPacket = sf::Packet();
    u8 cmdbyte = NetPacketCommand::CMD_FRAME_DATA_ACK;
    ackDataPacket.append(&cmdbyte, sizeof(cmdbyte));
    ackDataPacket.append(&ackData, sizeof(FrameAck));
    netplay->BroadcastPacket(ackDataPacket, ENET_PACKET_FLAG_UNSEQUENCED, server);
}

void CEXIBrawlback::ProcessIndividualRemoteFrameData(Match::PlayerFrameData* framedata) {
    // if the remote frame we're trying to process is not newer than the most recent frame, we don't care about it
    if (framedata->frame <= this->GetLatestRemoteFrame()) return; 

    // acknowledge that we received opponent's framedata
    BroadcastFramedataAck(framedata, this->netplay.get(), this->server);
    // ---------------------

    std::unique_ptr<Match::PlayerFrameData> f = std::make_unique<Match::PlayerFrameData>(*framedata);
    u8 playerIdx = f->playerIdx;
    s32 frame = (s32)f->frame;
    INFO_LOG(BRAWLBACK, "Received opponent framedata. Player %u frame: %u (w/o delay %u)\n", (unsigned int)playerIdx, frame, frame-FRAME_DELAY);

    std::lock_guard<std::mutex> lock (this->remotePadQueueMutex);
    PlayerFrameDataQueue& remoteFramedataQueue = this->remotePlayerFrameData[playerIdx];

    remoteFramedataQueue.push_back(std::move(f));
    if (!remoteFramedataQueue.empty()) {
        this->remotePlayerFrameDataMap[playerIdx][frame] = remoteFramedataQueue.back().get();
    }

    // clamp size of remote player framedata queue
    while (remoteFramedataQueue.size() > FRAMEDATA_MAX_QUEUE_SIZE) {
        //WARN_LOG(BRAWLBACK, "Hit remote player framedata queue max size! %u\n", remoteFramedataQueue.size());
        Match::PlayerFrameData* front_data = remoteFramedataQueue.front().release();
        if (this->remotePlayerFrameDataMap[playerIdx].count(front_data->frame)) {
            this->remotePlayerFrameDataMap[playerIdx].erase(front_data->frame);
        }
        delete front_data;
        remoteFramedataQueue.pop_front();
    }
}

void CEXIBrawlback::ProcessRemoteFrameData(Match::PlayerFrameData* framedatas, u8 numFramedatas_u8) {
    s32 numFramedatas = (s32)numFramedatas_u8;
    // framedatas may point to one or more PlayerFrameData's.
    // Also note. this array is the reverse of the local pad queue, in that
    // the 0th element here is the most recent framedata.
    Match::PlayerFrameData* mostRecentFramedata = &framedatas[0];

    if (numFramedatas > 0) {
        INFO_LOG(BRAWLBACK, "Received %i framedatas. Range: %u - %u \n", numFramedatas, mostRecentFramedata->frame, framedatas[numFramedatas-1].frame);
        
        this->timeSync->ReceivedRemoteFramedata(mostRecentFramedata->frame, mostRecentFramedata->playerIdx, this->hasGameStarted);

        this->numFramesWithoutRemoteInputs = 0;

        for (s32 i = numFramedatas-1; i >= 0; i--) {
            Match::PlayerFrameData* framedata = &framedatas[i];
            this->ProcessIndividualRemoteFrameData(framedata);
        }

        // if we've been using predicted inputs, and we just now received opponent framedata
        // framedatas will contain framedata for all frames we missed out on
        if (this->rollbackInfo.isUsingPredictedInputs) {
            this->rollbackInfo.endFrame = mostRecentFramedata->frame;
            INFO_LOG(BRAWLBACK, "Received remote inputs after having predicted inputs!\n");
            INFO_LOG(BRAWLBACK, "Num frames of input we just received: %i\n", numFramedatas);
            INFO_LOG(BRAWLBACK, "Rollback frame diff: %u - %u\n", this->rollbackInfo.endFrame, this->rollbackInfo.beginFrame);
            // set past framedatas
            memcpy(this->rollbackInfo.pastFrameDatas, framedatas, sizeof(Match::PlayerFrameData) * numFramedatas);
            this->rollbackInfo.pastFrameDataPopulated = true;
        }
    }

}

void CEXIBrawlback::ProcessGameSettings(Match::GameSettings* opponentGameSettings) {
    // merge game settings for all remote/local players, then pass that back to the game 

    this->localPlayerIdx = this->isHost ? 0 : 1;
    // assumes 1v1
    int remotePlayerIdx = this->isHost ? 1 : 0;

    Match::GameSettings* mergedGameSettings = this->gameSettings.get();
    INFO_LOG(BRAWLBACK, "ProcessGameSettings for player %u\n", this->localPlayerIdx);
    INFO_LOG(BRAWLBACK, "Remote player idx: %i\n", remotePlayerIdx);

    mergedGameSettings->localPlayerIdx = this->localPlayerIdx;

    this->numPlayers = mergedGameSettings->numPlayers;
    INFO_LOG(BRAWLBACK, "Num players from emu: %u\n", (unsigned int)this->numPlayers);

    // this is kinda broken kinda unstable and weird.
    // hardcoded "fix" for testing. Get rid of this when you're confident this is stable
    if (this->numPlayers == 0) {
        this->numPlayers = 2;
        mergedGameSettings->numPlayers = 2;
    }

    if (!this->isHost) {
        mergedGameSettings->randomSeed = opponentGameSettings->randomSeed;
        mergedGameSettings->stageID = opponentGameSettings->stageID;
    }
    mergedGameSettings->playerSettings[localPlayerIdx].playerType = Match::PlayerType::PLAYERTYPE_LOCAL;
    mergedGameSettings->playerSettings[remotePlayerIdx].playerType = Match::PlayerType::PLAYERTYPE_REMOTE;

    // if we're not host, we just connected to host and received their game settings, 
    // now we need to send our game settings back to them so they can start their game too
    if (!this->isHost) {
        this->netplay->BroadcastGameSettings(this->server, mergedGameSettings);
    }

    std::vector<u8> mergedGameSettingsByteVec = Mem::structToByteVector(mergedGameSettings);
    this->read_queue_mutex.lock();
    this->read_queue.push_back(EXICommand::CMD_SETUP_PLAYERS);
    this->read_queue.insert(this->read_queue.end(), mergedGameSettingsByteVec.begin(), mergedGameSettingsByteVec.end());
    this->read_queue_mutex.unlock();
}

void CEXIBrawlback::ProcessFrameAck(FrameAck* frameAck) {
    std::lock_guard<std::mutex> lock (this->ackTimersMutex);
    this->timeSync->ProcessFrameAck(frameAck, this->numPlayers, this->hasRemoteInputsThisFrame);
}

// called from netplay thread
void CEXIBrawlback::ProcessNetReceive(ENetEvent* event) {
    ENetPacket* pckt = event->packet;
    if (pckt && pckt->data && pckt->dataLength > 0) {
        u8* fullpckt_data = pckt->data;
        u8 cmd_byte = fullpckt_data[0];
        u8* data = &fullpckt_data[1];

        switch (cmd_byte) {
            case NetPacketCommand::CMD_FRAME_DATA:
                {
                    u8 numFramedatas = data[0];
                    Match::PlayerFrameData* framedata = (Match::PlayerFrameData*)(&data[1]);
                    this->ProcessRemoteFrameData(framedata, numFramedatas);
                }
                break;
            case NetPacketCommand::CMD_FRAME_DATA_ACK:
                {
                    FrameAck* frameAck = (FrameAck*)data;
                    this->ProcessFrameAck(frameAck);
                }
                break;
            case NetPacketCommand::CMD_GAME_SETTINGS:
                {
                    INFO_LOG(BRAWLBACK, "Received game settings from opponent");
                    Match::GameSettings* gameSettingsFromOpponent = (Match::GameSettings*)data;
                    this->ProcessGameSettings(gameSettingsFromOpponent);
                }
                break;
            default:
                WARN_LOG(BRAWLBACK, "Unknown packet cmd byte!");
                INFO_LOG(BRAWLBACK, "Packet as string: %s\n", fullpckt_data);
                break;
        }
    }
}

void CEXIBrawlback::NetplayThreadFunc() {
    ENetEvent event;
    bool isConnected = false;

    // loop until we connect to someone, then after we connected, 
    // do another loop for passing data between the connected clients
    
    INFO_LOG(BRAWLBACK, "Waiting for connection to opponent...");
    while (enet_host_service(this->server, &event, 0) >= 0 && !isConnected) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                INFO_LOG(BRAWLBACK, "Connected!");
                if (event.peer) {
                    INFO_LOG(BRAWLBACK, "A new client connected from %x:%u\n", 
                        event.peer -> address.host,
                        event.peer -> address.port);
                    isConnected = true;
                }
                else {
                    WARN_LOG(BRAWLBACK, "Connect event received, but peer was null!");
                }
                break;
            case ENET_EVENT_TYPE_NONE:
                //INFO_LOG(BRAWLBACK, "Enet event type none. Nothing to do");
                break;
        }
    }

    if (this->isHost) { // if we're host, send our gamesettings to clients right after connecting
        this->netplay->BroadcastGameSettings(this->server, this->gameSettings.get());
    }

    INFO_LOG(BRAWLBACK, "Starting main net data loop");
    // main enet loop
    while (enet_host_service(this->server, &event, 0) >= 0 && isConnected) {
        this->netplay->FlushAsyncQueue(this->server);
        switch (event.type) {
            case ENET_EVENT_TYPE_DISCONNECT:
                //INFO_LOG(BRAWLBACK, "%s:%u disconnected.\n", event.peer -> address.host, event.peer -> address.port);
                INFO_LOG(BRAWLBACK, "disconnected.\n");
                isConnected = false;
                break;
            case ENET_EVENT_TYPE_NONE:
                //INFO_LOG(BRAWLBACK, "Enet event type none. Nothing to do");
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                this->ProcessNetReceive(&event);
                enet_packet_destroy(event.packet);
                break;
        }
    }
    INFO_LOG(BRAWLBACK, "End enet thread");
}


void CEXIBrawlback::handleFindOpponent(u8* payload) {
    //if (!payload) return;

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = BRAWLBACK_PORT;

    this->server = enet_host_create(&address, 3, 0, 0, 0);

    // just for testing. This should be replaced with a check to see if we are the "host" of the match or not
    if (this->server == NULL) {
        this->isHost = false;
        WARN_LOG(BRAWLBACK, "Failed to init enet server!");
        WARN_LOG(BRAWLBACK, "Creating client instead...");
        this->server = enet_host_create(NULL, 3, 0, 0, 0);
        //for (int i = 0; i < 1; i++) { // make peers for all connecting opponents

            ENetAddress addr;
            int set_host_res = enet_address_set_host(&addr, "127.0.0.1");
            if (set_host_res < 0) {
                WARN_LOG(BRAWLBACK, "Failed to enet_address_set_host");
                return;
            }
            addr.port = BRAWLBACK_PORT;

            ENetPeer* peer = enet_host_connect(this->server, &addr, 1, 0);
            if (peer == NULL) {
                WARN_LOG(BRAWLBACK, "Failed to enet_host_connect");
                return;
            }

        //}
    }

    INFO_LOG(BRAWLBACK, "Net initialized, starting netplay thread");
    // loop to receive data over net
    this->netplay_thread = std::thread(&CEXIBrawlback::NetplayThreadFunc, this);
}


void CEXIBrawlback::handleStartMatch(u8* payload) {
    //if (!payload) return;
    Match::GameSettings* settings = (Match::GameSettings*)payload;
    this->gameSettings = std::make_unique<Match::GameSettings>(*settings);
}







// recieve data from game into emulator
void CEXIBrawlback::DMAWrite(u32 address, u32 size)
{
    //INFO_LOG(BRAWLBACK, "DMAWrite size: %u\n", size);
    u8* mem = Memory::GetPointer(address);

    if (!mem)
    {
        INFO_LOG(BRAWLBACK, "Invalid address in DMAWrite!");
        //this->read_queue.clear();
        return;
    }

    u8 command_byte = mem[0];  // first byte is always cmd byte
    u8* payload = &mem[1];     // rest is payload

    // no payload
    if (size <= 1) 
        payload = nullptr;


    switch (command_byte)
    {

    case CMD_UNKNOWN:
        INFO_LOG(BRAWLBACK, "Unknown DMAWrite command byte!");
        break;
    case CMD_ONLINE_INPUTS:
        //INFO_LOG(BRAWLBACK, "DMAWrite: CMD_ONLINE_INPUTS");
        handleLocalPadData(payload);
        break;
    case CMD_CAPTURE_SAVESTATE:
        //INFO_LOG(BRAWLBACK, "DMAWrite: CMD_CAPTURE_SAVESTATE");
        handleCaptureSavestate(payload);
        break;
    case CMD_LOAD_SAVESTATE:
        //INFO_LOG(BRAWLBACK, "DMAWrite: CMD_LOAD_SAVESTATE");
        handleLoadSavestate(payload);
        break;
    case CMD_FIND_OPPONENT:
        INFO_LOG(BRAWLBACK, "DMAWrite: CMD_FIND_OPPONENT");
        handleFindOpponent(payload);
        break;
    case CMD_START_MATCH:
        INFO_LOG(BRAWLBACK, "DMAWrite: CMD_START_MATCH");
        handleStartMatch(payload);
    default:
        //INFO_LOG(BRAWLBACK, "Default DMAWrite");
        break;
  }

}

// send data from emulator to game
void CEXIBrawlback::DMARead(u32 address, u32 size)
{
    std::lock_guard<std::mutex> lock(this->read_queue_mutex);

    if (this->read_queue.empty()) {// we have nothing to send to the game
        this->read_queue.push_back(EXICommand::CMD_UNKNOWN); // result code
    }

    // game is trying to get cmd byte
    if (size == 1) {
        Memory::CopyToEmu(address, &this->read_queue[0], size);
        this->read_queue.erase(this->read_queue.begin());
        return;
    }
    
    this->read_queue.resize(size, 0);
    auto qAddr = &this->read_queue[0];
    Memory::CopyToEmu(address, qAddr, size);
    this->read_queue.clear();

}





// honestly dunno why these are overriden like this, but slippi does it sooooooo  lol
bool CEXIBrawlback::IsPresent() const
{
    return true;
}

void CEXIBrawlback::TransferByte(u8& byte) { }
