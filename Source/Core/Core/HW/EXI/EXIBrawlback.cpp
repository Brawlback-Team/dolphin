

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
void CEXIBrawlback::handleLocalPadData(u8* data)
{
    Match::PlayerFrameData* playerFramedata = (Match::PlayerFrameData*)data;
    int idx = 0;
    // first 4 bytes are current game frame
    u32 frame = SlippiUtility::Mem::readWord(data, idx, 999, 0); // properly switch endianness
    //u32 frame_thing = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]; // properly switch endianness
    //playerFramedata->frame = Common::swap32(playerFramedata->frame); // properly switch endianness
    playerFramedata->frame = frame; // properly switch endianness


    std::unique_ptr<Match::PlayerFrameData> pFD = std::make_unique<Match::PlayerFrameData>(*playerFramedata);
    u32 frameWithDelay = frame >= FRAME_DELAY ? frame - FRAME_DELAY : 0; // clamp to 0
    INFO_LOG(BRAWLBACK, "Frame %u (w/delay %u) PlayerIdx: %u numPlayers %u\n", frame, frameWithDelay, playerFramedata->playerIdx, this->numPlayers);
    

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
                    // find framedata for FRAME_DELAY frames ago
                    if (this->localPlayerFrameData[i]->frame == frameWithDelay) {
                        INFO_LOG(BRAWLBACK, "found local inputs from %u frames ago (frame %u)\n", FRAME_DELAY, frameWithDelay);    
                        framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] = *(this->localPlayerFrameData[i].get());
                        foundData = true;
                    }
                }
                if (!foundData) {
                    framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] = CreateDummyPlayerFrameData(frameWithDelay, this->localPlayerIdx);
                }
                continue;
            }
            // search for remote player's inputs
            if (!this->remotePlayerFrameData.empty() && !this->remotePlayerFrameData[remotePlayerIdx].empty()) {
                PlayerFrameDataQueue& remotePlayerFrameDataQueue = this->remotePlayerFrameData[remotePlayerIdx];
                // find framedata in queue that has the frame we want to inject into the game (current frame - frame delay)
                INFO_LOG(BRAWLBACK, "Remote framedata q range: %u - %u\n", remotePlayerFrameDataQueue[0]->frame, remotePlayerFrameDataQueue[remotePlayerFrameDataQueue.size()-1]->frame);
                for (int i = 0; i < remotePlayerFrameDataQueue.size(); i++) {
                    if (remotePlayerFrameDataQueue[i]) {
                        Match::PlayerFrameData* framedata = remotePlayerFrameDataQueue[i].get();
                        // find framedata for this frame
                        if (framedata->frame == frame) {
                            INFO_LOG(BRAWLBACK, "found remote inputs: frame %u remotePlayerIdx: %u\n", framedata->frame, (unsigned int)remotePlayerIdx);
                            hasRemoteInputsThisFrame[remotePlayerIdx] = true;
                            framedataToSendToGame.playerFrameDatas[remotePlayerIdx] = *framedata;
                            foundData = true;
                            break;
                        }
                    }
                }
            }
                
            if (!foundData) { // didn't find framedata for this frame. Send most recent frame
                INFO_LOG(BRAWLBACK, "no remote framedata - frame %u remotePIdx %i\n", frame, remotePlayerIdx);
                //for (int b = 0; b < remotePlayerFrameDataQueue.size(); b++) {
                //    INFO_LOG(BRAWLBACK, "%u  %u\n", remotePlayerFrameDataQueue[b]->frame, (unsigned int)remotePlayerFrameDataQueue[b]->playerIdx);
                //}
                hasRemoteInputsThisFrame[remotePlayerIdx] = false;
                /*u32 searchEndFrame = frame >= MAX_ROLLBACK_FRAMES ? frame-MAX_ROLLBACK_FRAMES : 0;
                for (u32 frameIter = frame; frameIter >= searchEndFrame; frameIter--) { // find latest frame
                    if (this->remotePlayerFrameDataMap.count(frameIter)) {
                        framedataToSendToGame.playerFrameDatas[remotePlayerIdx] = *(*this->remotePlayerFrameDataMap[frameIter]).get();
                    }
                }*/
                framedataToSendToGame.playerFrameDatas[remotePlayerIdx] = CreateDummyPlayerFrameData(frame, remotePlayerIdx);
            }
            //else {
            //    INFO_LOG(BRAWLBACK, "Remote player %u frame data queue empty!\n", remotePlayerIdx);
            //}
        }

    }
    
    this->SendFrameDataToGame(&framedataToSendToGame);
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
    // acknowledge that we received opponent's framedata
    BroadcastFramedataAck(framedata, this->netplay.get(), this->server);
    // ---------------------
    std::unique_ptr<Match::PlayerFrameData> f = std::make_unique<Match::PlayerFrameData>(*framedata);

    std::lock_guard<std::mutex> lock (this->remotePadQueueMutex);
    u8 playerIdx = f->playerIdx;

    INFO_LOG(BRAWLBACK, "Received opponent framedata. Opponent idx: %u frame: %u\n", (unsigned int)f->playerIdx, f->frame);
    this->remotePlayerFrameData[playerIdx].push_back(std::move(f));
    //this->remotePlayerFrameDataMap[f->frame] = &this->remotePlayerFrameData[playerIdx].back();

    // get rid of old/non-relevant framedatas
    while (this->remotePlayerFrameData[playerIdx].size() > FRAMEDATA_QUEUE_SIZE) {
        Match::PlayerFrameData* front_data = this->remotePlayerFrameData[playerIdx].front().release();
        //if (this->remotePlayerFrameDataMap.count(front_data->frame)) {
            //this->remotePlayerFrameDataMap.erase(front_data->frame);
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
                    this->ackTimersMutex.lock();
                    u64 currentTime = Common::Timer::GetTimeUs();
                    FrameAck* frameAck = (FrameAck*)data;
                    u8 playerIdx = frameAck->playerIdx;
                    int frame = frameAck->frame;
                    INFO_LOG(BRAWLBACK, "Remote Frame acked %u pIdx %u", frame, (unsigned int)playerIdx);

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
                        break;
                    }
                    if (ackTimerFrontFrame != frame) {
                        INFO_LOG(BRAWLBACK, "frontframe and acked frame not equal\n");
                        break;
                    }

                    auto sendTime = this->ackTimers[playerIdx].front().timeUs;

                    this->ackTimers[playerIdx].pop_front();

                    // our ping is the current gametime - the time that the inputs were originally sent at
                    // inputs go from client 1 -> client 2 -> client 2 acks & sends ack to client 1 -> client 1 receives ack here
                    // so this is full RTT (round trip time). To get ping just from client to client, divide this by 2
                    this->pingUs[playerIdx] = currentTime - sendTime;

                    if (frame % PING_DISPLAY_INTERVAL == 0) {
                        std::stringstream pingStr;
                        pingStr << "Has Inputs: ";
                        for (int i = 0; i < this->numPlayers; i++)
                            pingStr << this->hasRemoteInputsThisFrame[i] << " | ";
                        pingStr << "Ping: ";
                        double ping = (double)this->pingUs[playerIdx] / 1000.0;
                        pingStr << ping << " ms";
                        OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, pingStr.str(), OSD::Duration::NORMAL, OSD::Color::GREEN);
                    }

                    this->ackTimersMutex.unlock();
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