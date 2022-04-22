#pragma once

#include <unordered_map>
#include <array>
#include <fstream>

#include "Common/FileUtil.h"
#include "Common/CommonTypes.h"
#include "Common/Timer.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "SlippiUtility.h"
#include "Core/Brawlback/Brawltypes.h"

// must be >= 1
#define FRAME_DELAY 3
static_assert(FRAME_DELAY + MAX_ROLLBACK_FRAMES >= 6); // 6 frames of "compensation" covers ~190 ping which is more than sufficient imo

#define ROLLBACK_IMPL true

// number of max FrameData's to keep in the queue
#define FRAMEDATA_MAX_QUEUE_SIZE 30 
// update ping display every X frames
#define PING_DISPLAY_INTERVAL 60

#define ONLINE_LOCKSTEP_INTERVAL 30
#define GAME_START_FRAME 0
//#define GAME_FULL_START_FRAME 1
#define GAME_FULL_START_FRAME 250

#define MAX_REMOTE_PLAYERS 3
#define MAX_NUM_PLAYERS 4
#define BRAWLBACK_PORT 7779



// 59.94 Hz (16.66 ms in a frame for 60fps)  ( -- is this accurate? This is the case for melee, idk if it also applies here)
//#define USEC_IN_FRAME 16683

#define MS_IN_FRAME (1000 / 60)
#define USEC_IN_FRAME (MS_IN_FRAME*1000)
#define MS_TO_FRAMES(ms) (ms * 60 / 1000)
#define FRAMES_TO_MS(frames) (1000 * frames / 60)

// ---
// mem dumping related
#include "Core/HW/AddressSpace.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
// ---

namespace Brawlback {

    struct UserInfo
    {
      std::string uid = "";
      std::string playKey = "";
      std::string displayName = "";
      std::string connectCode = "";
      std::string latestVersion = "";
      std::string fileContents = "";

      int port;
    };

    enum EXICommand : u8
    {
      CMD_UNKNOWN = 0,

      CMD_ONLINE_INPUTS = 1,
      CMD_CAPTURE_SAVESTATE = 2,
      CMD_LOAD_SAVESTATE = 3,

      CMD_FIND_OPPONENT = 5,
      CMD_START_MATCH = 13,
      CMD_SETUP_PLAYERS = 14,
      CMD_FRAMEDATA = 15,
      CMD_TIMESYNC = 16,
      CMD_ROLLBACK = 17,

      // REPLAYS (RECIEVE FROM GAME)
      CMD_REPLAY_INPUTS = 18,
      CMD_REPLAY_STAGE = 19,
      CMD_REPLAY_RANDOM = 20,
      CMD_REPLAY_FIGHTER = 21,
      CMD_REPLAY_GAME = 22,
      CMD_REPLAY_ENDGAME = 23,
      CMD_REPLAY_STARTPOS = 24,
      CMD_REPLAY_POS = 25,
      CMD_REPLAY_STARTFIGHTER = 26,
      CMD_REPLAY_STICK = 27,
      CMD_REPLAY_ACTIONSTATE = 28,
      CMD_REPLAY_ITEM_IDS = 29,
      CMD_REPLAY_ITEM_VARIENTS = 30,
      CMD_REPLAY_NUM_PLAYERS = 31,
      CMD_REPLAY_STOCK_COUNT = 32,
      CMD_REPLAY_CURRENT_INDEX = 33,
      CMD_REPLAY_GET_NUMBER_REPLAY_FILES = 34,
      CMD_REPLAY_GET_REPLAY_FILES = 37,
      CMD_REPLAY_GET_REPLAY_FILES_SIZE = 38,
      CMD_REPLAY_GET_REPLAY_NAMES = 40,
      CMD_REPLAY_GET_REPLAY_NAMES_SIZE = 42,

      // REPLAYS (SEND TO GAME)
      CMD_REPLAY_SEND_NUMBER_REPLAY_FILES = 35,
      CMD_REPLAY_SEND_REPLAY_FILES = 36,
      CMD_REPLAY_SEND_REPLAY_FILES_SIZE = 39,
      CMD_REPLAY_SEND_REPLAY_NAMES = 41,
      CMD_REPLAY_SEND_REPLAY_NAMES_SIZE = 43,

      CMD_GET_MATCH_STATE = 4,
      CMD_SET_MATCH_SELECTIONS = 6,

      CMD_TIMER_START = 7,
      CMD_TIMER_END = 8,
      CMD_UPDATE = 9,
      
      CMD_GET_ONLINE_STATUS = 10,
      CMD_CLEANUP_CONNECTION = 11,
      CMD_GET_NEW_SEED = 12,
    };

    enum NetPacketCommand : u8 
    {
        CMD_FRAME_DATA = 1,
        CMD_GAME_SETTINGS = 2,
        CMD_FRAME_DATA_ACK = 3,
    };

    struct FrameOffsetData {
        int idx;
        std::vector<s32> buf;
    };
    enum itemIdName
    {
      Assist_Trophy = 0x00,
      Franklin_Badge = 0x01,
      Banana_Peel = 0x02,
      Barrel = 0x03,
      Beam_Sword = 0x04,
      Bill_Coin_mode = 0x05,
      Bob_Omb = 0x06,
      Crate = 0x07,
      Bumper = 0x08,
      Capsule = 0x09,
      Rolling_Crate = 0x0A,
      CD = 0x0B,
      Gooey_Bomb = 0x0C,
      Cracker_Launcher = 0x0D,
      Cracker_Launcher_Shot = 0x0E,
      Coin = 0x0F,
      Superspicy_Curry = 0x10,
      Superspice_Curry_Shot = 0x11,
      Deku_Nut = 0x12,
      Mr_Saturn = 0x13,
      Dragoon_Part = 0x14,
      Dragoon_Set = 0x15,
      Dragoon_Sight = 0x16,
      Trophy = 0x17,
      Fire_Flower = 0x18,
      Fire_Flower_Shot = 0x19,
      Freezie = 0x1A,
      Golden_Hammer = 0x1B,
      Green_Shell = 0x1C,
      Hammer = 0x1D,
      Hammer_Head = 0x1E,
      Fan = 0x1F,
      Heart_Container = 0x20,
      Homerun_Bat = 0x21,
      Party_Ball = 0x22,
      Manaphy_Heart = 0x23,
      Maxim_Tomato = 0x24,
      Poison_Mushroom = 0x25,
      Super_Mushroom = 0x26,
      Metal_Box = 0x27,
      Hothead = 0x28,
      Pitfall = 0x29,
      Pokeball = 0x2A,
      Blast_Box = 0x2B,
      Ray_Gun = 0x2C,
      Ray_Gun_Shot = 0x2D,
      Lipstick = 0x2E,
      Lipstick_Flower = 0x2F,
      Lipstick_Shot_Dust_Powder = 0x30,
      Sandbag = 0x31,
      Screw_Attack = 0x32,
      Sticker = 0x33,
      Motion_Sensor_Bomb = 0x34,
      Timer = 0x35,
      Smart_Bomb = 0x36,
      Smash_Ball = 0x37,
      Smoke_Screen = 0x38,
      Spring = 0x39,
      Star_Rod = 0x3A,
      Star_Rod_Shot = 0x3B,
      Soccer_Ball = 0x3C,
      Super_Scope = 0x3D,
      Super_Scope_shot = 0x3E,
      Star = 0x3F,
      Food = 0x40,
      Team_Healer = 0x41,
      Lightning = 0x42,
      Unira = 0x43,
      Bunny_Hood = 0x44,
      Warpstar = 0x45,
      Trophy_SSE = 0x46,
      Key = 0x47,
      Trophy_Stand = 0x48,
      Stock_Ball = 0x49,
      Apple_Green_Greens = 0x4A,
      Sidestepper = 0x4B,
      Shellcreeper = 0x4C,
      Pellet = 0x4D,
      Vegetable_Summit = 0x4E,
      Sandbag_HRC = 0x4F,
      Auroros = 0x50,
      Koopa1 = 0x51,
      Koopa2 = 0x52,
      Snakes_Box = 0x53,
      Diddys_Peanut = 0x54,
      Links_Bomb = 0x55,
      Peachs_Turnup = 0x56,
      ROBs_Gyro = 0x57,
      Seed_edible_peanut = 0x58,
      Snakes_Grenade = 0x59,
      Samus_Armor_piece = 0x5A,
      Toon_Links_Bomb = 0x5B,
      Warios_Bike = 0x5C,
      Warios_Bike_A = 0x5D,
      Warios_Bike_B = 0x5E,
      Warios_Bike_C = 0x5F,
      Warios_Bike_D = 0x60,
      Warios_Bike_E = 0x61,
      Torchic = 0x62,
      Cerebi = 0x63,
      Chickorita = 0x64,
      Chickoritas_Shot = 0x65,
      Entei = 0x66,
      Moltres = 0x67,
      Munchlax = 0x68,
      Deoxys = 0x69,
      Groudon = 0x6A,
      Gulpin = 0x6B,
      Staryu = 0x6C,
      Staryus_Shot = 0x6D,
      Ho_oh = 0x6E,
      Ho_ohs_Shot = 0x6F,
      Jirachi = 0x70,
      Snorlax = 0x71,
      Bellossom = 0x72,
      Kyogre = 0x73,
      Kyogres_Shot = 0x74,
      Latias_and_Latios = 0x75,
      Lugia = 0x76,
      Lugias_Shot = 0x77,
      Manaphy = 0x78,
      Weavile = 0x79,
      Electrode = 0x7A,
      Metagross = 0x7B,
      Mew = 0x7C,
      Meowth = 0x7D,
      Meowths_Shot = 0x7E,
      Piplup = 0x7F,
      Togepi = 0x80,
      Goldeen = 0x81,
      Gardevoir = 0x82,
      Wobuffet = 0x83,
      Suicune = 0x84,
      Bonsly = 0x85,
      Andross = 0x86,
      Andross_Shot = 0x87,
      Barbara = 0x88,
      Gray_Fox = 0x89,
      Ray_MKII_Custom_Robo = 0x8A,
      Ray_MKII_Bomb = 0x8B,
      Ray_MKII_Gun_Shot = 0x8C,
      Samurai_Goroh = 0x8D,
      Devil = 0x8E,
      Excitebike = 0x8F,
      Jeff_Andonuts = 0x90,
      Jeff_Pencil_Bullet = 0x91,
      Jeff_Pencil_Rocket = 0x92,
      Lakitu = 0x93,
      Knuckle_Joe = 0x94,
      Knuckle_Joe_Shot = 0x95,
      Hammer_Bro = 0x96,
      Hammer_Bro_Hammer = 0x97,
      Helirin = 0x98,
      Kat_and_Ana = 0x99,
      Ana = 0x9A,
      Jill_and_Drill_Dozer = 0x9B,
      Lyn = 0x9C,
      Little_Mac = 0x9D,
      Metroid = 0x9E,
      Nintendog = 0x9F,
      NintendogFull = 0xA0,
      Mr_Resetti = 0xA1,
      Isaac = 0xA2,
      Isaac_Shot = 0xA3,
      Saki_Amamiya = 0xA4,
      Saki_Shot_1 = 0xA5,
      Saki_Shot_2 = 0xA6,
      Shadow_the_Hedgehog = 0xA7,
      Infantry = 0xA8,
      Infantry_Shot = 0xA9,
      Stafy = 0xAA,
      Tank_Infantry = 0xAB,
      Tank_Shot = 0xAC,
      Tingle = 0xAD,
      togezo = 0xAE,
      Waluigi = 0xAF,
      Dr_Wright = 0xB0,
      Wright_Buildings = 0xB1,
      Unknown1 = 0x7D1,
      Unknown2 = 0x7D2,
      Unknown3 = 0x7D3,
      Unknown4 = 0x7D4,
      Unknown5 = 0x7D5
    };
    namespace Match
    {   
        struct PlayerFrameDataImpl {
            PlayerFrameData _playerFrameData;

            // do these impact the size of the struct?
            // wouldn't the vtable ptr screw with it being interpreted on gameside???
            // (since the gameside structs don't have these ctors)
            PlayerFrameDataImpl() {
                _playerFrameData.frame = 0;
                _playerFrameData.playerIdx = 0;
                _playerFrameData.pad = BrawlbackPadImpl()._brawlbackPad;
            }
            PlayerFrameDataImpl(u32 _frame, u8 _playerIdx)
            {
                _playerFrameData.frame = _frame;
                _playerFrameData.playerIdx = _playerIdx;
                _playerFrameData.pad = BrawlbackPadImpl()._brawlbackPad;
            }
        };

        struct FrameDataImpl {
            FrameData _frameData;

            FrameDataImpl()
            {
                _frameData.randomSeed = 0;
                for (int i = 0; i < MAX_NUM_PLAYERS; i++) {
                    _frameData.playerFrameDatas[i] = PlayerFrameDataImpl()._playerFrameData;
                }
            }
            FrameDataImpl(u32 frame)
            {
                _frameData.randomSeed = 0;
                for (u8 i = 0; i < MAX_NUM_PLAYERS; i++) {
                    _frameData.playerFrameDatas[i] = PlayerFrameDataImpl(frame, i)._playerFrameData;
                }
            }
        };
        struct PlayerSettingsImpl
        {
            PlayerSettings _playerSettings;
        };

        struct GameSettingsImpl
        {
            GameSettings _gameSettings;
        };
        struct Game {
            u32 version;
            GameSettingsImpl gameSettings;
            u32 currentFrame;
            std::unordered_map<int32_t, FrameDataImpl*> framesByIndex;
            std::vector<std::unique_ptr<FrameDataImpl>> frames;
        };

        struct RollbackInfoImpl {
            RollbackInfo _rollbackInfo;
            std::vector<SlippiUtility::Savestate::PreserveBlockImpl> preserveBlocks;

            RollbackInfoImpl() {
                Reset();
            }
            void Reset() {
                _rollbackInfo.isUsingPredictedInputs = false;
                _rollbackInfo.beginFrame = 0;
                _rollbackInfo.endFrame = 0;
                _rollbackInfo.predictedInputs = FrameDataImpl()._frameData;
                _rollbackInfo.pastFrameDataPopulated = false;
                memset(_rollbackInfo.pastFrameDatas, 0, sizeof(FrameData) * MAX_ROLLBACK_FRAMES);
                _rollbackInfo.hasPreserveBlocks = false;
                preserveBlocks = {};
            }

        };

        bool isPlayerFrameDataEqual(const PlayerFrameData& p1, const PlayerFrameData& p2);
        
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
    namespace Sync {
        std::string getSyncLogFilePath();
        std::string str_byte(uint8_t byte);
        std::string str_half(u16 half);
        void SyncLog(const std::string& msg);
        std::string stringifyFramedata(const Match::PlayerFrameDataImpl& pfd);
    }
    
    typedef std::deque<std::unique_ptr<Match::PlayerFrameDataImpl>> PlayerFrameDataQueue;

    Match::PlayerFrameDataImpl* findInPlayerFrameDataQueue(const PlayerFrameDataQueue& queue,
                                                           u32 frame);

    template <typename T>
    T Clamp(T input, T Max, T Min) {
        return input > Max ? Max : ( input < Min ? Min : input );
    }

    namespace Dump {
        void DoMemDumpIteration(int& dump_num);
        void DumpMem(AddressSpace::Type memType, const std::string& dumpPath);
    }

}
