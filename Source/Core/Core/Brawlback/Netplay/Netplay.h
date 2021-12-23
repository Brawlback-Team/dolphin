#pragma once

#include <SFML/Network/Packet.hpp>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
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

    typedef std::pair<sf::Packet, int> BrawlbackNetPacket;

    void SendAsync(std::unique_ptr<BrawlbackNetPacket> packet, ENetHost* host);
    void BroadcastPacket(sf::Packet& packet, int enet_flag, ENetHost* server);
    void FlushAsyncQueue(ENetHost* server);

}

