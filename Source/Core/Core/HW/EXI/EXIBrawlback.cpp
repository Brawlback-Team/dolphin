

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


// `data` is a ptr to a FrameData struct
void CEXIBrawlback::handlePadData(u8* data)
{
    int idx = 0;
    // first 4 bytes are current game frame
    u32 frame = SlippiUtility::Mem::readWord(data, idx, 999, 0); // does this change the endianness?
    u32 randomSeed = SlippiUtility::Mem::readWord(data, idx, 999, 0);

    //INFO_LOG(BRAWLBACK, "\nFrom emu: Game frame: %u\nRandom seed: %u\n", frame, randomSeed);
    gfPadGamecube* pads = (gfPadGamecube*)&data[idx];

    // init FrameData
    std::unique_ptr<Match::FrameData> pFD = std::make_unique<Match::FrameData>();
    pFD->frame = frame;
    pFD->randomSeed = randomSeed;
    for (int i = 0; i < 4; i++) 
    {
        memcpy(&pFD->pads[i], &pads[i], sizeof(gfPadGamecube));
    }


    // store framedata
    if (this->playersFrameData.size() + 1 > MAX_ROLLBACK_FRAMES) 
    {
        this->playersFrameData.pop_front();
    }
    this->playersFrameData.push_back(std::move(pFD));


    // send framedata to all peers
    if (this->server) {
        sf::Packet frame_data_packet = sf::Packet();

        // append cmd byte
        u8 frame_data_cmd = NetPacketCommand::CMD_FRAME_DATA;
        frame_data_packet.append(&frame_data_cmd, sizeof(u8));

        // append framedata
        Match::FrameData* framedata = this->playersFrameData.back().get();
        frame_data_packet.append(framedata, sizeof(Match::FrameData));

        // send framedata to other oppponent(s)
        ENetPacket* enetPckt = enet_packet_create(frame_data_packet.getData(), frame_data_packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(this->server, 0, enetPckt); // "broadcast" here means send to all connected peers.
    }


}

void CEXIBrawlback::ProcessRemoteFrameData(Match::FrameData* framedata) {
    // copy data, since the ptr passed in here will get freed at the end of the enet loop

    // ehhhh idk if this is necessary, the bytes might get copied during the read_queue insertion anyway
    Match::FrameData* tmp_fd = (Match::FrameData*)malloc(sizeof(Match::FrameData)); // not thread safe lol
    memcpy(tmp_fd, framedata, sizeof(Match::FrameData));

    std::vector<u8> frame_data_bytes = Mem::structToByteVector(tmp_fd);

    this->read_queue_mutex.lock();
    this->read_queue.clear();
    this->read_queue.push_back(EXICommand::CMD_FRAMEDATA);
    this->read_queue.insert(this->read_queue.end(), frame_data_bytes.begin(), frame_data_bytes.end());
    this->read_queue_mutex.unlock();
}

// called from netplay thread
void CEXIBrawlback::ProcessNetPacket(ENetPacket* pckt) {
    if (pckt && pckt->data && pckt->dataLength > 0) {
        u8* fullpckt_data = pckt->data;

        u8 cmd_byte = fullpckt_data[0];
        u8* data = &fullpckt_data[1];

        switch (cmd_byte) {
            case NetPacketCommand::CMD_FRAME_DATA:
                {
                    //INFO_LOG(BRAWLBACK, "Received frame data from opponent!\n");
                    Match::FrameData* framedata = (Match::FrameData*)data;
                    this->ProcessRemoteFrameData(framedata);
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

    INFO_LOG(BRAWLBACK, "Starting main net data loop");

    // main enet loop
    ENetEvent event;
    bool isConnected = true;
    while (enet_host_service(this->server, &event, 0) >= 0 && isConnected) {
        switch (event.type) {
            case ENET_EVENT_TYPE_DISCONNECT:
                INFO_LOG(BRAWLBACK, "%s:%u disconnected.\n", event.peer -> address.host, event.peer -> address.port);
                isConnected = false;
                break;
            case ENET_EVENT_TYPE_NONE:
                //INFO_LOG(BRAWLBACK, "Enet event type none. Nothing to do");
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                this->ProcessNetPacket(event.packet);
                enet_packet_destroy(event.packet);
                break;
        }
    }



    INFO_LOG(BRAWLBACK, "End enet thread");
}


void CEXIBrawlback::handleFindOpponent(u8* payload) {
    //if (!payload) return;
    //bool isHost = payload[0];

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
        for (int i = 0; i < 1; i++) {

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

        }
    }


    INFO_LOG(BRAWLBACK, "Net initialized, starting net main loop");

    // first do a loop to find opponent
    ENetEvent event;
    bool isConnected = false;

    INFO_LOG(BRAWLBACK, "Waiting for connection to opponent...");
    // loop until we connect to someone, then after we connected, do another loop for passing data between the connected clients
    while (enet_host_service(this->server, &event, 0) >= 0 && !isConnected) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                INFO_LOG(BRAWLBACK, "Connected!");
                if (event.peer) {
                    INFO_LOG(BRAWLBACK, "A new client connected from %x:%u.\n", 
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

}

void CEXIBrawlback::handleStartMatch(u8* payload) {
    //if (!payload) return;

    std::unique_ptr<Match::GameSettings> gameSettings = std::make_unique<Match::GameSettings>();

    
    
    // if this is only a 1v1
    u8 localPlayerIdx = this->isHost ? 0 : 1;

    // let game know which player is the local player
    this->read_queue_mutex.lock();
    this->read_queue.push_back(EXICommand::CMD_SETUP_PLAYERS);
    this->read_queue.push_back(localPlayerIdx);
    this->read_queue_mutex.unlock();

    // loop to receive data over net
    netplay_thread = std::thread(&CEXIBrawlback::NetplayThreadFunc, this);
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
        handlePadData(payload);
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
    if (this->read_queue_mutex.try_lock()) {
        if (this->read_queue.empty()) {// we have nothing to send to the game
            this->read_queue.push_back(EXICommand::CMD_UNKNOWN); // result code
        }
        this->read_queue.resize(size, 0);
        auto qAddr = &this->read_queue[0];
        Memory::CopyToEmu(address, qAddr, size);
        this->read_queue.clear();
        this->read_queue_mutex.unlock();
    }
}





// honestly dunno why these are overriden like this, but slippi does it sooooooo  lol
bool CEXIBrawlback::IsPresent() const
{
    return true;
}

void CEXIBrawlback::TransferByte(u8& byte) { }