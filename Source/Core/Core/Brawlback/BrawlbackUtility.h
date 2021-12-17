#pragma once

#include <unordered_map>
#include <array>

#include "Common/CommonTypes.h"
#include "Core/Brawlback/Brawltypes.h"

#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"

#include "SlippiUtility.h"



namespace Brawlback {
    const u8 NAMETAG_SIZE = 8;
    const u8 DISPLAY_NAME_SIZE = 31;
    const u8 CONNECT_CODE_SIZE = 10;

    


    namespace Match
    {

        enum PlayerType : u8
        {
            PLAYERTYPE_LOCAL = 0x0,
            PLAYERTYPE_REMOTE = 0x1,
        };

        // info stored about each player every frame
        //#pragma pack(4)
        struct FrameData {
            u32 frame;
            u32 randomSeed;
            gfPadGamecube pads[4];
        };
        //#pragma pack()

        struct PlayerSettings
        {
            u8 charID;
            u8 charColor;
            PlayerType playerType;
            u8 controllerPort;
            std::array<uint16_t, NAMETAG_SIZE> nametag;
            std::array<uint8_t, DISPLAY_NAME_SIZE> displayName;
            std::array<uint8_t, CONNECT_CODE_SIZE> connectCode;
        };

        struct GameSettings
        {
            u16 stageID;
            u32 randomSeed;
            std::vector<PlayerSettings> playerSettings;
        };

        struct Game {
            u32 version;
            GameSettings gameSettings;
            u32 currentFrame;
            std::unordered_map<int32_t, FrameData*> framesByIndex;
            std::vector<std::unique_ptr<FrameData>> frames;
        };
    }




    // checks if the specified `button` is held down based on the buttonBits bitfield
    bool isButtonPressed(u16 buttonBits, PADButtonBits button);

    namespace Mem {
        void print_byte(uint8_t byte);
        void print_half(u16 half);
        void print_word(u32 word);

        template <typename T>
        std::vector<u8> structToByteVector(T* s) {
            auto ptr = reinterpret_cast<u8*>(s);
            return std::vector<u8>(ptr, ptr + sizeof(T));
        }

        void fillByteVectorWithBuffer(std::vector<u8>& vec, u8* buf, size_t size);
    }

}
