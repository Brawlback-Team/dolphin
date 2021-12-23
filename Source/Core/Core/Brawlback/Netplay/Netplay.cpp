#include "Netplay.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <thread>
#include "Common/CommonTypes.h"
#include "Common/ENetUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"



namespace Netplay {

    std::deque<std::unique_ptr<BrawlbackNetPacket>> async_queue;
    std::recursive_mutex async_send_packet_mutex;


    void SendAsync(std::unique_ptr<BrawlbackNetPacket> packet, ENetHost* host) {
        {
            std::lock_guard<std::recursive_mutex> lock(async_send_packet_mutex);
            async_queue.push_back(std::move(packet));
        }
        ENetUtil::WakeupThread(host);
    }

    void BroadcastPacket(sf::Packet& packet, int enet_flag, ENetHost* server) {
        ENetPacket* p = enet_packet_create(packet.getData(), packet.getDataSize(), enet_flag);
		enet_host_broadcast(server, 0, p);
    }

    void FlushAsyncQueue(ENetHost* server) {
        while (!async_queue.empty())
		{
            BrawlbackNetPacket packet = *(async_queue.front().get());
			BroadcastPacket(packet.first, packet.second, server);
			async_queue.pop_front();
		}
    }



}
