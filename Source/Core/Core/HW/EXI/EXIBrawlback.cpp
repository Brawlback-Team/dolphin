

#include "EXIBrawlback.h"

#include "Core/HW/Memmap.h"
#include <chrono>
#include "VideoCommon/OnScreenDisplay.h"



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
    std::unique_ptr<BrawlbackSavestate> ss = std::make_unique<BrawlbackSavestate>();

    if (savestates.size() + 1 > MAX_ROLLBACK_FRAMES)
    {
      savestates.pop_front(); // pop savestate from front of queue if we would go over the max # of rollback frames
    }


    auto start = std::chrono::high_resolution_clock::now();
    ss->Capture();
    auto finish = std::chrono::high_resolution_clock::now();


    std::chrono::duration<double> elapsed = finish - start;
    //INFO_LOG(BRAWLBACK, "Capture Savestate took %f\n", elapsed.count());

    savestates.push_back(std::move(ss));
  
}

void CEXIBrawlback::handleLoadSavestate(u8* data)
{
    // Fetch preservation blocks
    std::vector<PreserveBlock> blocks = {};

    if (data) // populate preservation blocks
    {
        // first 4 bytes are game frame
        //s32 frame = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
        //INFO_LOG(BRAWLBACK, "Loading savestate for frame: %u\n", frame);
        // rest of data is preservation blocks
        u32* preserveArr = (u32*)(&data[4]);

        int idx = 0;
        while (Common::swap32(preserveArr[idx]) != 0)
        {
            // each PreserveBlock is made up of 8 bytes. 4 for address and 4 for length
            PreserveBlock p = {Common::swap32(preserveArr[idx]), Common::swap32(preserveArr[idx + 1])};
            blocks.push_back(p);
            idx += 2;  // increment by 8 bytes
        }
    }

    if (!savestates.empty())
    {
        auto start = std::chrono::high_resolution_clock::now();
        savestates.front()->Load(blocks);
        auto finish = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed = finish - start;
        //INFO_LOG(BRAWLBACK, "Load Savestate took %f\n", elapsed.count());
    }
    else
    {
        INFO_LOG(BRAWLBACK, "Empty savestate queue when trying to load state!");
    }
 
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
    u32 frame = SlippiUtility::Mem::readWord(data, idx, 999, 0); // properly switch endianness
    //u8 playerIdx = playerFramedata->playerIdx;
    playerFramedata->frame = frame; // properly switch endianness

    if (frame == GAME_START_FRAME) {
        for (u32 i = frame; i <= frame+FRAME_DELAY; i++) {
            Match::PlayerFrameData dummyFramedata = CreateDummyPlayerFrameData(i, this->localPlayerIdx);
            this->netplay->BroadcastPlayerFrameData(this->server, &dummyFramedata);
        }
    }

    std::unique_ptr<Match::PlayerFrameData> pFD = std::make_unique<Match::PlayerFrameData>(*playerFramedata);
    // local inputs offset by FRAME_DELAY to mask latency
    // Once we hit frame X, we send inputs for that frame, but pretend they're from frame X+2
    // so those inputs now have an extra 2 frames to get to the opponent before the opponent's
    // client hits frame X+2. 
    u32 frameWithDelay = frame + FRAME_DELAY; 
    pFD->frame = frameWithDelay;
    INFO_LOG(BRAWLBACK, "Frame %u PlayerIdx: %u numPlayers %u\n", frame, playerFramedata->playerIdx, this->numPlayers);
    

    {   // store the time that we sent framedata
        std::lock_guard<std::mutex> lock(this->ackTimersMutex);
        u64 currentTime = Common::Timer::GetTimeUs();
        for (int i = 0; i < this->numPlayers; i++) {
            FrameTiming timing;
            timing.frame = frame;
            timing.timeUs = currentTime;

            this->lastFrameTimings[i] = timing;
            this->ackTimers[i].push_back(timing);
        }
    }

    // store local framedata
    if (this->localPlayerFrameData.size() + 1 > FRAMEDATA_QUEUE_SIZE) {
        Match::PlayerFrameData* front_pfd = this->localPlayerFrameData.front().release();
        delete front_pfd;
        this->localPlayerFrameData.pop_front();
    }
    this->localPlayerFrameData.push_back(std::move(pFD));

    // broadcast this local framedata
    if (!this->localPlayerFrameData.empty()) {
        this->netplay->BroadcastPlayerFrameData(this->server, this->localPlayerFrameData.back().get()); // copies player framedata
    }

    Match::FrameData framedataToSendToGame = Match::FrameData();
    framedataToSendToGame.randomSeed = 0x496ffd00; // tmp

    {
        std::lock_guard<std::mutex> lock (this->remotePadQueueMutex);

        // for each remote player
        for (int remotePlayerIdx = 0; remotePlayerIdx < this->numPlayers; remotePlayerIdx++) {
            bool foundData = false;
            // search for local player's inputs
            if (remotePlayerIdx == this->localPlayerIdx && !this->localPlayerFrameData.empty()) {
                for (int i = 0; i < this->localPlayerFrameData.size(); i++) {
                    // find local framedata for this frame
                    if (this->localPlayerFrameData[i]->frame == frame) {
                        INFO_LOG(BRAWLBACK, "found local inputs\n");
                        framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] = *(this->localPlayerFrameData[i].get());
                        foundData = true;
                    }
                }
                if (!foundData) {
                    framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] = CreateDummyPlayerFrameData(frame, this->localPlayerIdx);
                }
                continue;
            }
            // search for remote player's inputs
            if (!this->remotePlayerFrameData.empty() && !this->remotePlayerFrameData[remotePlayerIdx].empty()) {
                PlayerFrameDataQueue& remotePlayerFrameDataQueue = this->remotePlayerFrameData[remotePlayerIdx];
                // find framedata in queue that has the frame we want to inject into the game (current frame - frame delay)
                INFO_LOG(BRAWLBACK, "Remote framedata q range: %u - %u\n", remotePlayerFrameDataQueue.front()->frame, remotePlayerFrameDataQueue.back()->frame);
                for (int i = 0; i < remotePlayerFrameDataQueue.size(); i++) {
                    if (remotePlayerFrameDataQueue[i]) {
                        Match::PlayerFrameData* framedata = remotePlayerFrameDataQueue[i].get();
                        // find remote framedata for this frame
                        if (framedata->frame == frame) {
                            INFO_LOG(BRAWLBACK, "found remote inputs\n");
                            hasRemoteInputsThisFrame[remotePlayerIdx] = true;
                            framedataToSendToGame.playerFrameDatas[remotePlayerIdx] = *framedata;
                            foundData = true;
                            break;
                        }
                    }
                }
            }
                
            if (!foundData) { // didn't find framedata for this frame.
                INFO_LOG(BRAWLBACK, "no remote framedata - frame %u remotePIdx %i\n", frame, remotePlayerIdx);
                hasRemoteInputsThisFrame[remotePlayerIdx] = false;
                framedataToSendToGame.playerFrameDatas[remotePlayerIdx] = CreateDummyPlayerFrameData(frame, remotePlayerIdx);
            }
        }

    }
    


    if (this->shouldStallFrame(frame)) {
        this->SendTimeSyncToGame();
    }
    else {
        this->SendFrameDataToGame(&framedataToSendToGame);
    }

    //this->SendFrameDataToGame(&framedataToSendToGame);
}

void CEXIBrawlback::SendTimeSyncToGame() {
    std::lock_guard<std::mutex> lock (this->read_queue_mutex);
    this->read_queue.clear();
    this->read_queue.push_back(EXICommand::CMD_TIMESYNC);
}

bool CEXIBrawlback::shouldStallFrame(u32 frame) {
    if (this->isConnectionStalled) return false;

    u32 latestFrame = this->GetLatestRemoteFrame();

    INFO_LOG(BRAWLBACK, "local/remote frame difference - local: %u  remote %u\n", frame, latestFrame);

    if (frame - latestFrame > MAX_ROLLBACK_FRAMES) {
        this->stallFrameCount += 1;
        if (this->stallFrameCount > 60 * 7) {
            isConnectionStalled = true;
        }
        INFO_LOG(BRAWLBACK, "Exceeded rollback lim, halting for one frame. Frame: %u  Latest %u\n", frame, latestFrame);
        return true;
    }

    stallFrameCount = 0;

    // Return true if we are over 60% of a frame ahead of our opponent. Currently limiting how
	// often this happens because I'm worried about jittery data causing a lot of unneccesary delays.
	// Only skip once for a given frame because our time detection method doesn't take into consideration
	// waiting for a frame. Also it's less jarring and it happens often enough that it will smoothly
	// get to the right place
	auto isTimeSyncFrame = frame % ONLINE_LOCKSTEP_INTERVAL; // Only time sync every 30 frames
	if (isTimeSyncFrame == 0 && !isSkipping)
	{
		s32 offsetUs = this->calcTimeOffsetUs();
		INFO_LOG(BRAWLBACK, "[Frame %u] Offset is: %d us", frame, offsetUs);

		// TODO: figure out a better solution here for doubles?
		if (offsetUs > 10000)
		{
			isSkipping = true;

			int maxSkipFrames = frame <= 120 ? 5 : 1; // On early frames, support skipping more frames
			framesToSkip = ((offsetUs - 10000) / MS_IN_FRAME) + 1;
			framesToSkip = framesToSkip > maxSkipFrames ? maxSkipFrames : framesToSkip; // Only skip 5 frames max

			WARN_LOG(BRAWLBACK, "Halting on frame %d due to time sync. Offset: %d us. Frames: %d...", frame,
			         offsetUs, framesToSkip);
		}
	}

	// Handle the skipped frames
	if (framesToSkip > 0)
	{
		// If ahead by 60% of a frame, stall. I opted to use 60% instead of half a frame
		// because I was worried about two systems continuously stalling for each other
		framesToSkip = framesToSkip - 1;
		return true;
	}

	isSkipping = false;

	return false;
}

s32 CEXIBrawlback::calcTimeOffsetUs() {
    bool empty = true;
	for (int i = 0; i < this->numPlayers; i++)
	{
		if (!frameOffsetData[i].buf.empty())
		{
			empty = false;
			break;
		}
	}
	if (empty)
	{
		return 0;
	}

	std::vector<int> offsets;
	for (int pIdx = 0; pIdx < this->numPlayers; pIdx++)
	{
		if (frameOffsetData[pIdx].buf.empty())
			continue;

		std::vector<s32> buf;
		std::copy(frameOffsetData[pIdx].buf.begin(), frameOffsetData[pIdx].buf.end(), std::back_inserter(buf));

		// TODO: Does this work?
		std::sort(buf.begin(), buf.end());

		int bufSize = (int)buf.size();
		int offset = (int)((1.0f / 3.0f) * bufSize);
		int end = bufSize - offset;

		int sum = 0;
		for (int i = offset; i < end; i++)
		{
			sum += buf[i];
		}

		int count = end - offset;
		if (count <= 0)
		{
			return 0; // What do I return here?
		}

		s32 result = sum / count;
		offsets.push_back(result);
	}

	s32 maxOffset = offsets.front();
	for (int i = 1; i < offsets.size(); i++)
	{
		if (offsets[i] > maxOffset)
			maxOffset = offsets[i];
	}

	// INFO_LOG(SLIPPI_ONLINE, "Time offsets, [0]: %d, [1]: %d, [2]: %d", offsets[0], offsets[1], offsets[2]);
	return maxOffset;
}

u32 CEXIBrawlback::GetLatestRemoteFrame() {
    u32 lowestFrame = 0;
	for (int i = 0; i < this->numPlayers; i++)
	{
        if (i == this->localPlayerIdx) continue;

		if (remotePlayerFrameData[i].empty())
		{
			return 0;
		}

		u32 f = remotePlayerFrameData[i].back()->frame;
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
    u8 cmdbyte = CEXIBrawlback::NetPacketCommand::CMD_FRAME_DATA_ACK;
    ackDataPacket.append(&cmdbyte, sizeof(cmdbyte));
    ackDataPacket.append(&ackData, sizeof(FrameAck));
    netplay->BroadcastPacket(ackDataPacket, ENET_PACKET_FLAG_UNSEQUENCED, server);
}

void CEXIBrawlback::ProcessRemoteFrameData(Match::PlayerFrameData* framedata) {
    u64 curTime = Common::Timer::GetTimeUs();

    // acknowledge that we received opponent's framedata
    BroadcastFramedataAck(framedata, this->netplay.get(), this->server);
    // ---------------------

    std::unique_ptr<Match::PlayerFrameData> f = std::make_unique<Match::PlayerFrameData>(*framedata);
    u8 playerIdx = f->playerIdx;
    u32 frame = f->frame;


    // --------------- TIME SYNC STUFF ---------------
    if (frame == GAME_START_FRAME) this->hasGameStarted = true;

    // update frame timing/offsets for time sync logic
    
    // Pad received, try to guess what our local time was when the frame was sent by our opponent
    // before we initialized
    // We can compare this to when we sent a pad for last frame to figure out how far/behind we
    // are with respect to the opponent
    auto timing = lastFrameTimings[playerIdx];
    if (!hasGameStarted)
    {
        // Handle case where opponent starts sending inputs before our game has reached frame 1. This will
        // continuously say frame 0 is now to prevent opp from getting too far ahead
        timing.frame = 0;
        timing.timeUs = curTime;
    }

    s64 opponentSendTimeUs = curTime - (pingUs[playerIdx] / 2);
    s64 frameDiffOffsetUs = MS_IN_FRAME * (timing.frame - frame);
    s64 timeOffsetUs = opponentSendTimeUs - timing.timeUs + frameDiffOffsetUs;

    INFO_LOG(BRAWLBACK, "[Offset] Opp Frame: %d, My Frame: %d. Time offset: %lld", frame, timing.frame,
             timeOffsetUs);

    // Add this offset to circular buffer for use later
    if (frameOffsetData[playerIdx].buf.size() < ONLINE_LOCKSTEP_INTERVAL)
        frameOffsetData[playerIdx].buf.push_back((s32)timeOffsetUs);
    else
        frameOffsetData[playerIdx].buf[frameOffsetData[playerIdx].idx] = (s32)timeOffsetUs;

    frameOffsetData[playerIdx].idx = (frameOffsetData[playerIdx].idx + 1) % ONLINE_LOCKSTEP_INTERVAL;

    // --------------- END TIME SYNC STUFF ---------------


    std::lock_guard<std::mutex> lock (this->remotePadQueueMutex);

    INFO_LOG(BRAWLBACK, "Received opponent framedata. Opponent idx: %u frame: %u\n", (unsigned int)playerIdx, frame);
    this->remotePlayerFrameData[playerIdx].push_back(std::move(f));
    //if (!this->remotePlayerFrameData.empty()) {
    //    this->remotePlayerFrameDataMap[playerIdx][frame] = (u32)this->remotePlayerFrameData.size()-1;
    //}

    // get rid of old/non-relevant framedatas
    while (this->remotePlayerFrameData[playerIdx].size() > FRAMEDATA_QUEUE_SIZE) {
        Match::PlayerFrameData* front_data = this->remotePlayerFrameData[playerIdx].front().release();
        //if (this->remotePlayerFrameDataMap[playerIdx].count(front_data->frame)) {
        //    this->remotePlayerFrameDataMap[playerIdx].erase(front_data->frame);
        //}
        delete front_data;
        this->remotePlayerFrameData[playerIdx].pop_front();
    }

}

void CEXIBrawlback::SendFrameDataToGame(Match::FrameData* framedata) {
    // send framedata down to game for injection
    std::vector<u8> frame_data_bytes = Mem::structToByteVector(framedata);
    this->read_queue_mutex.lock();
    this->read_queue.clear();
    this->read_queue.push_back(EXICommand::CMD_FRAMEDATA);
    // copies each byte
    this->read_queue.insert(this->read_queue.end(), frame_data_bytes.begin(), frame_data_bytes.end());
    this->read_queue_mutex.unlock();
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

    u64 currentTime = Common::Timer::GetTimeUs();
    u8 playerIdx = frameAck->playerIdx;
    int frame = frameAck->frame;

    int lastAcked = this->lastFrameAcked[playerIdx];
    // if this current acked frame is more recent than the last acked frame, set it
    this->lastFrameAcked[playerIdx] = lastAcked < frame ? frame : lastAcked;
    int ackTimerFrontFrame = this->ackTimers[playerIdx].front().frame;

    // remove old timings
    while (!this->ackTimers[playerIdx].empty() && ackTimerFrontFrame < frame) {
        this->ackTimers[playerIdx].pop_front();
    }

    if (this->ackTimers[playerIdx].empty()) {
        INFO_LOG(BRAWLBACK, "Empty acktimers\n");
        return;
    }
    if (ackTimerFrontFrame != frame) {
        INFO_LOG(BRAWLBACK, "frontframe and acked frame not equal\n");
        return;
    }

    auto sendTime = this->ackTimers[playerIdx].front().timeUs;

    this->ackTimers[playerIdx].pop_front();

    // our ping is the current gametime - the time that the inputs were originally sent at
    // inputs go from client 1 -> client 2 -> client 2 acks & sends ack to client 1 -> client 1 receives ack here
    // so this is full RTT (round trip time). To get ping just from client to client, divide this by 2
    this->pingUs[playerIdx] = currentTime - sendTime;
    u64 rtt = this->pingUs[playerIdx];

    INFO_LOG(BRAWLBACK, "Remote Frame acked %u pIdx %u rtt %llu\n", frame, (unsigned int)playerIdx, rtt);

    if (frame % PING_DISPLAY_INTERVAL == 0) {
        std::stringstream dispStr;
        dispStr << "Has Remote Inputs: ";
        for (int i = 0; i < this->numPlayers; i++)
            dispStr << this->hasRemoteInputsThisFrame[i] << " | ";
        dispStr << "Rtt: ";
        double ping = (double)rtt / 1000.0;
        dispStr << ping << " ms";
        OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, dispStr.str(), OSD::Duration::NORMAL, OSD::Color::GREEN);
    }

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
                    //INFO_LOG(BRAWLBACK, "Received frame data from opponent!\n");
                    Match::PlayerFrameData* framedata = (Match::PlayerFrameData*)data;
                    this->ProcessRemoteFrameData(framedata);
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
                INFO_LOG(BRAWLBACK, "%s:%u disconnected.\n", event.peer -> address.host, event.peer -> address.port);
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
    //this->isHost = payload[0];

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
