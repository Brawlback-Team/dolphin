#pragma once
#include "Core/HW/EXI/EXI_Device.h"
#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <memory>
#include <deque>
#include "Core/Brawlback/Savestate.h"

#define MAX_ROLLBACK_FRAMES 7

enum PADButtonBits
{
  Start = 0x1000,
  Y = 0x800,
  X = 0x400,
  B = 0x200,
  A = 0x100,
  L = 0x40,
  R = 0x20,
  Z = 0x10,
  UpDPad = 0x8,
  DownDPad = 0x4,
  RightDPad = 0x2,
  LeftDPad = 0x1
};

#pragma pack(2)
union PADButtons
{
  unsigned short bits;
  struct
  {
    unsigned _none : 3;
    unsigned Start : 1;
    unsigned Y : 1;
    unsigned X : 1;
    unsigned B : 1;
    unsigned A : 1;
    unsigned _none2 : 1;
    unsigned L : 1;
    unsigned R : 1;
    unsigned Z : 1;
    unsigned UpDPad : 1;
    unsigned DownDPad : 1;
    unsigned RightDPad : 1;
    unsigned LeftDPad : 1;
  };
};

#pragma pack(4)
struct gfPadGamecube
{
  char _spacer[6];
  // 0x6
  PADButtons buttons;
  char _spacer2[0x30 - 6 - sizeof(PADButtons)];
  // 0x30
  char stickX;
  char stickY;
  char cStickX;
  char cStickY;
  char LTrigger;
  char RTrigger;

  char _spacer3[0x38 - 0x30 - 6];
  // 0x38
  // 0xFF if not connected, else 0
  bool isNotConnected;

  char _spacer4[0x3C - 0x38 - 1];

  // 0x3C
  int type = 0;
};

class CEXIBrawlback : public ExpansionInterface::IEXIDevice
{

  public:
    CEXIBrawlback();
    ~CEXIBrawlback() override;

    
    void DMAWrite(u32 address, u32 size) override;
    void DMARead(u32 address, u32 size) override;

    bool IsPresent() const;


  private:

    // byte vector for sending into to the game
    std::vector<u8> read_queue = {};

    enum EXICommand
    {
      CMD_UNKNOWN = 0,
      // Online
      CMD_ONLINE_INPUTS = 1,
      CMD_CAPTURE_SAVESTATE = 2,
      CMD_LOAD_SAVESTATE = 3,
      CMD_GET_MATCH_STATE = 4,
      CMD_FIND_OPPONENT = 5,
      CMD_SET_MATCH_SELECTIONS = 6,
      CMD_OPEN_LOGIN = 7,
      CMD_LOGOUT = 8,
      CMD_UPDATE = 9,
      CMD_GET_ONLINE_STATUS = 10,
      CMD_CLEANUP_CONNECTION = 11,
      CMD_SEND_CHAT_MESSAGE = 12,
      CMD_GET_NEW_SEED = 13,
      CMD_REPORT_GAME = 14,
    };


    std::unordered_map<u8, u32> payloadSizes = {
        // The following are used for Slippi online and also have fixed sizes
        {CMD_ONLINE_INPUTS, 17},
        {CMD_CAPTURE_SAVESTATE, 32},
        {CMD_LOAD_SAVESTATE, 32},
        {CMD_GET_MATCH_STATE, 0},
        {CMD_FIND_OPPONENT, 19},
        {CMD_SET_MATCH_SELECTIONS, 8},
        {CMD_SEND_CHAT_MESSAGE, 2},
        {CMD_OPEN_LOGIN, 0},
        {CMD_LOGOUT, 0},
        {CMD_UPDATE, 0},
        {CMD_GET_ONLINE_STATUS, 0},
        {CMD_CLEANUP_CONNECTION, 0},
        {CMD_GET_NEW_SEED, 0},
        {CMD_REPORT_GAME, 16},
    };

    void handleCaptureSavestate(u8* data);


    std::vector<gfPadGamecube> pads = {};

    std::deque<std::unique_ptr<BrawlbackSavestate>> savestates;




};










// UTIL


namespace Brawlback
{
  namespace Mem
  {
    std::vector<u8> uint16ToVector(u16 num);
    std::vector<u8> uint32ToVector(u32 num);
    std::vector<u8> int32ToVector(int32_t num);
    void appendWordToBuffer(std::vector<u8>* buf, u32 word);
    void appendHalfToBuffer(std::vector<u8>* buf, u16 word);

    uint8_t readByte(uint8_t* a, int& idx, uint32_t maxSize, uint8_t defaultValue);
    uint16_t readHalf(uint8_t* a, int& idx, uint32_t maxSize, uint16_t defaultValue);
    uint32_t readWord(uint8_t* a, int& idx, uint32_t maxSize, uint32_t defaultValue);
    float readFloat(uint8_t* a, int& idx, uint32_t maxSize, float defaultValue);

    void print_byte(uint8_t byte);
  }  // namespace Mem
}  // namespace Brawlback
