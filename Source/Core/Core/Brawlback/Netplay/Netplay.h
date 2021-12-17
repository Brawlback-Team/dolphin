#pragma once

#include <SFML/Network/Packet.hpp>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "InputCommon/GCPadStatus.h"
#include "Core/Brawlback/BrawlbackUtility.h"


namespace Netplay {



    void handleFindOpponent(u8* payload);
    void handleStartMatch(u8* payload);
    void receiveEvents();




/* BrawlbackNetplay {

public:

    BrawlbackNetplay();
    ~BrawlbackNetplay();

    void handleFindOpponent(u8* payload);
    void handleStartMatch(u8* payload);

private:

    void receiveEvents();

    ENetHost* server;
    std::vector<ENetHost*> clients;

};


extern BrawlbackNetplay* BBNetplay;
*/
}

