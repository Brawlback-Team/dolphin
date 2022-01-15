

#include "EXIBrawlback.h"

#include "Core/HW/Memmap.h"
#include <chrono>

// --- Mutexes
std::mutex read_queue_mutex = std::mutex();
std::mutex remotePadQueueMutex = std::mutex();
std::mutex localPadQueueMutex = std::mutex();
// -------------------------------

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
    u64 startTime = Common::Timer::GetTimeUs();

    int idx = 0;
    s32 frame = (s32)SlippiUtility::Mem::readWord(data, idx, 999, 0);

    // tmp
    if (frame == GAME_START_FRAME && availableSavestates.empty()) {
        for (int i = 0; i < MAX_ROLLBACK_FRAMES; i++) {
			availableSavestates.push_back(std::make_unique<BrawlbackSavestate>());
		}
    }

    // Grab an available savestate
	std::unique_ptr<BrawlbackSavestate> ss;
	if (!availableSavestates.empty())
	{
		ss = std::move(availableSavestates.back());
		availableSavestates.pop_back();
	}
	else
	{
		// If there were no available savestates, use the oldest one
		auto it = activeSavestates.begin();
		ss = std::move(it->second);
		activeSavestates.erase(it->first);
	}

	// If there is already a savestate for this frame, remove it and add it to available
	if (activeSavestates.count(frame))
	{
		availableSavestates.push_back(std::move(activeSavestates[frame]));
		activeSavestates.erase(frame);
	}

	ss->Capture();
	activeSavestates[frame] = std::move(ss);

	u32 timeDiff = (u32)(Common::Timer::GetTimeUs() - startTime);
    INFO_LOG(BRAWLBACK, "Captured savestate for frame %d in: %f ms", frame, ((double)timeDiff) / 1000);
}

void CEXIBrawlback::handleLoadSavestate(u8* data)
{

    Match::RollbackInfo* loadStateRollbackInfo = (Match::RollbackInfo*)data;
    // the frame we should load state for is the frame we first began not receiving inputs
    //u32 frame = Common::swap32(loadStateRollbackInfo->beginFrame);
    s32 frame = (s32)SlippiUtility::Mem::readWord((u8*)&loadStateRollbackInfo->beginFrame);

    INFO_LOG(BRAWLBACK, "Attempting to load state for frame %i\n", frame);

    //u32* preserveArr = (u32*)loadStateRollbackInfo->preserveBlocks.data();

	if (!activeSavestates.count(frame))
	{
		// This savestate does not exist... uhhh? What do we do?
        PanicAlertFmtT("Savestate for frame {0} does not exist.", frame);
        this->isConnected = false;
		return;
	}

	u64 startTime = Common::Timer::GetTimeUs();

	// Fetch preservation blocks
	std::vector<PreserveBlock> blocks = {};

    /*
	// Get preservation blocks
	int idx = 0;
	while (Common::swap32(preserveArr[idx]) != 0)
	{
		PreserveBlock p = {Common::swap32(preserveArr[idx]), Common::swap32(preserveArr[idx + 1])};
		blocks.push_back(p);
		idx += 2;
	}
    */

	// Load savestate
	activeSavestates[frame]->Load(blocks);

	// Move all active savestates to available
	for (auto it = activeSavestates.begin(); it != activeSavestates.end(); ++it)
	{
		availableSavestates.push_back(std::move(it->second));
	}

	activeSavestates.clear();

	u32 timeDiff = (u32)(Common::Timer::GetTimeUs() - startTime);
    INFO_LOG(BRAWLBACK, "Loaded savestate for frame %d in: %f ms", frame, ((double)timeDiff) / 1000);
}

template <typename T>
void CEXIBrawlback::SendCmdToGame(EXICommand cmd, T* payload) {
    //std::lock_guard<std::mutex> lock (read_queue_mutex); // crash here wtf?
    this->read_queue.clear();
    this->read_queue.push_back(cmd);
    if (payload) {
        std::vector<u8> byteVec = Mem::structToByteVector(payload);
        this->read_queue.insert(this->read_queue.end(), byteVec.begin(), byteVec.end());
    }
}

void CEXIBrawlback::SendCmdToGame(EXICommand cmd) {
    //std::lock_guard<std::mutex> lock (read_queue_mutex);
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
    this->gameFrame = frame;
    u8 playerIdx = playerFramedata->playerIdx;
    playerFramedata->frame = frame; // properly switched endianness

    if (frame == GAME_START_FRAME) {
        availableSavestates.clear();
		activeSavestates.clear();

		// Prepare savestates
		for (int i = 0; i < MAX_ROLLBACK_FRAMES; i++) {
			availableSavestates.push_back(std::make_unique<BrawlbackSavestate>());
		}

        // push framedatas for first few delay frames
        for (int i = GAME_START_FRAME; i <= FRAME_DELAY; i++) {
            this->remotePlayerFrameData[playerIdx].push_back(std::make_unique<Match::PlayerFrameData>(CreateDummyPlayerFrameData(i, playerIdx)));
            this->localPlayerFrameData.push_back(std::make_unique<Match::PlayerFrameData>(CreateDummyPlayerFrameData(i, playerIdx)));
        }
        this->hasGameStarted = true;
    }


    bool shouldTimeSync = this->timeSync->shouldStallFrame(frame, this->GetLatestRemoteFrame(), this->numPlayers);
    if (shouldTimeSync) {
        INFO_LOG(BRAWLBACK, "Sending time sync command for this frame\n");
        // Send inputs that have not yet been acked
        this->handleSendInputs(frame);
        this->SendCmdToGame(EXICommand::CMD_TIMESYNC);
        return;
    }


    // store these local inputs (with frame delay)
    this->storeLocalInputs(playerFramedata);
    // broadcasts local inputs
    this->handleSendInputs(frame);

    // getting inputs for all players & sending them to game
    Match::FrameData framedataToSendToGame = Match::FrameData(frame);
    framedataToSendToGame.randomSeed = 0x496ffd00; // tmp
    // populates playerFrameDatas field of the above FrameData
    std::pair<bool, bool> foundData = this->getInputsForGame(framedataToSendToGame, frame);

    #if ROLLBACK_IMPL
    if (this->rollbackInfo.pastFrameDataPopulated) {
        //INFO_LOG(BRAWLBACK, "Past frame data is populated! Sending rollback cmd to game\n");
        this->SendCmdToGame(EXICommand::CMD_ROLLBACK, &this->rollbackInfo);
        this->rollbackInfo.Reset(); // reset rollbackInfo
        return;
    }
    #endif

    if (!foundData.second && frame > FRAME_DELAY) { // if we didn't find remote inputs AND didn't find/use predicted inputs for some reason
        INFO_LOG(BRAWLBACK, "Sending time sync command for this frame\n");
        this->SendCmdToGame(EXICommand::CMD_TIMESYNC);
    }
    else {
        this->SendCmdToGame(EXICommand::CMD_FRAMEDATA, &framedataToSendToGame);
    }

}

void CEXIBrawlback::storeLocalInputs(Match::PlayerFrameData* localPlayerFramedata) {
    std::lock_guard<std::mutex> local_lock (localPadQueueMutex);
    std::unique_ptr<Match::PlayerFrameData> pFD = std::make_unique<Match::PlayerFrameData>(*localPlayerFramedata);
    // local inputs offset by FRAME_DELAY to mask latency
    // Once we hit frame X, we send inputs for that frame, but pretend they're from frame X+2
    // so those inputs now have an extra 2 frames to get to the opponent before the opponent's
    // client hits frame X+2. 
    pFD->frame = localPlayerFramedata->frame + FRAME_DELAY;
    //INFO_LOG(BRAWLBACK, "Frame %u PlayerIdx: %u numPlayers %u\n", localPlayerFramedata->frame, localPlayerFramedata->playerIdx, this->numPlayers);
    
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
        std::lock_guard<std::mutex> local_lock (localPadQueueMutex);

        // this strat is taken from slippi [ thanks fizzi <3 ]
        // each frame we send local inputs to the other client(s)
        // those clients then acknowledge those inputs and send that ack(nowledgement)
        // back to us. All the inputs that are acked shouldn't be kept track of in the
        // local pad queue since we know for a fact the remote client has them/
        // Inputs that *are* kept track of in the local pad queue are inputs that
        // we don't know for sure the remote client has received.
        // that's why we send *all* local inputs with every packet.
        // so that when the remote client doesn't receive inputs, and needs to rollback
        // the next packet will have all the inputs that that client hasn't received.
        this->DropAckedInputs(frame);

        int localPadQueueSize = (int)this->localPlayerFrameData.size();
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
    u32 frameWithDelay = frame + FRAME_DELAY;
    // TODO (pine):
    // holy shit clean up this mess please

    std::lock_guard<std::mutex> lock (remotePadQueueMutex);

    // first is if we've found local inputs, second is if we've found remote inputs
    std::pair<bool, bool> foundData = std::make_pair(false, false);

    // for each player
    for (int playerIdx = 0; playerIdx < this->numPlayers; playerIdx++) {
        //bool foundData = false;
        // search for local player's inputs
        if (playerIdx == this->localPlayerIdx && !this->localPlayerFrameData.empty()) {
            std::lock_guard<std::mutex> local_lock (localPadQueueMutex);
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
                // this shouldn't ever happen lol. Just have this here so things don't go totally haywire
                WARN_LOG(BRAWLBACK, "Couldn't find local inputs! Using empty pad.\n");
                WARN_LOG(BRAWLBACK, "Local pad input range: [%u - %u]\n", this->localPlayerFrameData.back()->frame, this->localPlayerFrameData.front()->frame);
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
                        this->numFramesWithoutRemoteInputs = 0;

                        if (this->rollbackInfo.isUsingPredictedInputs && frame > GAME_FULL_START_FRAME) {
                            this->SetupRollback(frame);
                        }
                        
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

            //                                      tmp testing - don't rollback on early frames to avoid weird stuff with file loads
                if (!this->rollbackInfo.isUsingPredictedInputs && frame > GAME_FULL_START_FRAME) {
                    INFO_LOG(BRAWLBACK, "Trying to find frame for predicted inputs...\n");

                    // frame? Or frameWithDelay?
                    u32 searchEndFrame = frameWithDelay >= MAX_ROLLBACK_FRAMES ? frameWithDelay - MAX_ROLLBACK_FRAMES : 0; // clamp to 0
                    // iterate MAX_ROLLBACK_FRAMES into the past to find player framedata
                    // this is where we """"predict""""" player inputs when we don't receive them.
                    for (u32 frameIter = frameWithDelay; frameIter > searchEndFrame; frameIter--) { 
                        // find most recent frame that exists
                        if (this->remotePlayerFrameDataMap[playerIdx].count(frameIter)) {
                            INFO_LOG(BRAWLBACK, "found frame for predicting inputs %u\n", frameIter);
                            Match::PlayerFrameData* mostRecentFramedata = this->remotePlayerFrameDataMap[playerIdx][frameIter];
                            // copy it into the framedata that'll be sent to the game
                            framedataToSendToGame.playerFrameDatas[playerIdx] = *mostRecentFramedata;

                            foundData.second = true;

                            // set rollback info
                            this->rollbackInfo.isUsingPredictedInputs = true;
                            this->rollbackInfo.beginFrame = frame;
                            this->rollbackInfo.predictedInputs.playerFrameDatas[playerIdx] = *mostRecentFramedata;
                            break;
                        }
                    }

                    if (!foundData.second) {
                        INFO_LOG(BRAWLBACK, "Searched %u - %u   remote framedata range: %u - %u\n", 
                        searchEndFrame, frameWithDelay, this->remotePlayerFrameData[playerIdx].front()->frame, this->remotePlayerFrameData[playerIdx].back()->frame);
                        // couldn't find relevant past framedata
                        // this probably means the difference between clients is greater than MAX_ROLLBACK_FRAMES
                        // foundData.second will be false, so this should time-sync later in handleLocalPadData
                        WARN_LOG(BRAWLBACK, "Couldn't find framedata when we should rollback!! Will timesync\n");
                    }
                }
                else {
                    
                    /*static size_t prevRemoteQueueSize = 0;
                    size_t newRemoteQueueSize = this->remotePlayerFrameData[playerIdx].size();
                    if (prevRemoteQueueSize != newRemoteQueueSize && frame > GAME_FULL_START_FRAME) { // received new inputs
                        this->SetupRollback(frame);
                    }
                    prevRemoteQueueSize = newRemoteQueueSize;*/
                    
                    
                    
                    // we've already encountered a frame without inputs, and have set rollbackinfo, so just use those predicted inputs
                    Match::PlayerFrameData* predictedInputs = &this->rollbackInfo.predictedInputs.playerFrameDatas[playerIdx];
                    INFO_LOG(BRAWLBACK, "Using predicted inputs from frame %u\n", predictedInputs->frame);
                    framedataToSendToGame.playerFrameDatas[playerIdx] = *predictedInputs;
                    foundData.second = true;
                    this->numFramesWithoutRemoteInputs += 1;

                    // if we've been predicting for more than max rollback frames
                    if (frame - predictedInputs->frame >= MAX_ROLLBACK_FRAMES) {
                        INFO_LOG(BRAWLBACK, "Num frames without remote inputs exceedes max rollback frames\n");
                        // let parent func know that we "havent found remote inputs" so it'll timesync
                        foundData.second = false;
                    }


                }

            }
            else {
                ERROR_LOG(BRAWLBACK, "Too early of a frame. Can't rollback. Using dummy pad\n");
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


// prepares RollbackInfo struct with relevant rollback info
void CEXIBrawlback::SetupRollback(u32 frame) {
    this->rollbackInfo.endFrame = frame;
    INFO_LOG(BRAWLBACK, "Received remote inputs after having predicted inputs!\n");
    INFO_LOG(BRAWLBACK, "Rollback frame diff: %u - %u\n", this->rollbackInfo.endFrame, this->rollbackInfo.beginFrame);

    for (int pIdx = 0; pIdx < this->numPlayers; pIdx++) {
        if (pIdx == this->localPlayerIdx) continue;

        for (u32 i = this->rollbackInfo.beginFrame; i <= this->rollbackInfo.endFrame; i++) {

            u32 frameDiff = this->rollbackInfo.endFrame - i;

            // copy in past remote inputs
            if (this->remotePlayerFrameDataMap[pIdx].count(i)) {

                Match::PlayerFrameData* pastFramedata = this->remotePlayerFrameDataMap[pIdx][i];
                memcpy(&this->rollbackInfo.pastFrameDatas[frameDiff].playerFrameDatas[pIdx], pastFramedata, sizeof(Match::PlayerFrameData));
                INFO_LOG(BRAWLBACK, "Found remote inputs for rollback frame %u\n", i);
                this->rollbackInfo.pastFrameDataPopulated = true;
            
            }
            else {
                INFO_LOG(BRAWLBACK, "couldn't find remote input frame %u\n", i);
            }
        }
    }

    // TODO (pine):
    // copy local inputs in here too?
    // nah. That would further complicate things with local input queue and acks.
    // do what slippi does and just keep some past local framedatas gameside and populate
    // with those on a rollback
}

void CEXIBrawlback::DropAckedInputs(u32 currFrame) {
    // Remove pad reports that have been received and acked
    u32 minAckFrame = (u32)this->timeSync->getMinAckFrame(this->numPlayers);
    //if (minAckFrame > currFrame) {
    //    INFO_LOG(BRAWLBACK, "minAckFrame > currFrame      %u > %u\n", minAckFrame, currFrame);
    //}
    // BANDAID SOLUTION - TODO: FIX FOR REAL
    minAckFrame = minAckFrame > currFrame ? currFrame : minAckFrame; // clamp to current frame to prevent it dropping local inputs that haven't been used yet
    
    INFO_LOG(BRAWLBACK, "Checking to drop local inputs, oldest frame: %d | minAckFrame: %u",
                this->localPlayerFrameData.front()->frame, minAckFrame);
    INFO_LOG(BRAWLBACK, "Local input queue frame range: %u - %u\n", this->localPlayerFrameData.front()->frame, this->localPlayerFrameData.back()->frame);
    
    while (!this->localPlayerFrameData.empty() && this->localPlayerFrameData.front()->frame < minAckFrame)
    {
        INFO_LOG(BRAWLBACK, "Dropping local input for frame %d from queue", this->localPlayerFrameData.front()->frame);
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

void BroadcastFramedataAck(u32 frame, u8 playerIdx, BrawlbackNetplay* netplay, ENetHost* server) {
    FrameAck ackData;
    ackData.frame = (int)frame;
    ackData.playerIdx = playerIdx;
    sf::Packet ackDataPacket = sf::Packet();
    u8 cmdbyte = NetPacketCommand::CMD_FRAME_DATA_ACK;
    ackDataPacket.append(&cmdbyte, sizeof(cmdbyte));
    ackDataPacket.append(&ackData, sizeof(FrameAck));
    netplay->BroadcastPacket(ackDataPacket, ENET_PACKET_FLAG_UNSEQUENCED, server);
}





void CEXIBrawlback::ProcessIndividualRemoteFrameData(Match::PlayerFrameData* framedata) {
    u8 playerIdx = framedata->playerIdx;
    u32 frame = framedata->frame;
    PlayerFrameDataQueue& remoteFramedataQueue = this->remotePlayerFrameData[playerIdx];

    // if the remote frame we're trying to process is not newer than the most recent frame, we don't care about it
    if (!remoteFramedataQueue.empty() && frame <= remoteFramedataQueue.back()->frame) return; 

    std::unique_ptr<Match::PlayerFrameData> f = std::make_unique<Match::PlayerFrameData>(*framedata);
    //INFO_LOG(BRAWLBACK, "Received opponent framedata. Player %u frame: %u (w/o delay %u)\n", (unsigned int)playerIdx, frame, frame-FRAME_DELAY);

    remoteFramedataQueue.push_back(std::move(f));
    if (!remoteFramedataQueue.empty()) { //
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
    u8 playerIdx = mostRecentFramedata->playerIdx;

    if (!this->remotePlayerFrameData[playerIdx].empty())
      INFO_LOG(BRAWLBACK, "Received remote inputs. Head frame %u  received head frame %u\n", this->remotePlayerFrameData[playerIdx].back()->frame, mostRecentFramedata->frame);

    if (numFramedatas > 0) {
        std::lock_guard<std::mutex> lock (remotePadQueueMutex);
        INFO_LOG(BRAWLBACK, "Received %i framedatas. Range: [%u - %u]\n", numFramedatas, framedatas[numFramedatas-1].frame, mostRecentFramedata->frame);
        
        u32 maxFrame = 0;
        for (s32 i = numFramedatas-1; i >= 0; i--) {
            Match::PlayerFrameData* framedata = &framedatas[i];
            if (framedata->frame > maxFrame) {
                maxFrame = framedata->frame;
            }
            this->ProcessIndividualRemoteFrameData(framedata);
        }

        this->timeSync->ReceivedRemoteFramedata(maxFrame, playerIdx, this->hasGameStarted);

        // acknowledge that we received opponent's framedata
        BroadcastFramedataAck(maxFrame, playerIdx, this->netplay.get(), this->server);
        // ---------------------
    }

}

void CEXIBrawlback::ProcessFrameAck(FrameAck* frameAck) {
    this->timeSync->ProcessFrameAck(frameAck, this->hasRemoteInputsThisFrame[1 - frameAck->playerIdx]);
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
    std::lock_guard<std::mutex> lock (read_queue_mutex);
    this->read_queue.push_back(EXICommand::CMD_SETUP_PLAYERS);
    this->read_queue.insert(this->read_queue.end(), mergedGameSettingsByteVec.begin(), mergedGameSettingsByteVec.end());
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

    // loop until we connect to someone, then after we connected, 
    // do another loop for passing data between the connected clients
    
    INFO_LOG(BRAWLBACK, "Waiting for connection to opponent...");
    while (enet_host_service(this->server, &event, 0) >= 0 && !this->isConnected) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                INFO_LOG(BRAWLBACK, "Connected!");
                if (event.peer) {
                    INFO_LOG(BRAWLBACK, "A new client connected from %x:%u\n", 
                        event.peer -> address.host,
                        event.peer -> address.port);
                    this->isConnected = true;
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
    while (enet_host_service(this->server, &event, 0) >= 0 && this->isConnected && !this->timeSync->getIsConnectionStalled()) {
        this->netplay->FlushAsyncQueue(this->server);
        switch (event.type) {
            case ENET_EVENT_TYPE_DISCONNECT:
                //INFO_LOG(BRAWLBACK, "%s:%u disconnected.\n", event.peer -> address.host, event.peer -> address.port);
                INFO_LOG(BRAWLBACK, "disconnected.\n");
                this->isConnected = false;
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
    std::lock_guard<std::mutex> lock(read_queue_mutex);

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
