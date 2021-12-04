

#include "EXIBrawlback.h"

#include "Core/HW/Memmap.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include <chrono>




CEXIBrawlback::CEXIBrawlback()
{
  INFO_LOG(BRAWLBACK, "BRAWLBACK exi ctor");
}

CEXIBrawlback::~CEXIBrawlback()
{

}




void CEXIBrawlback::handleCaptureSavestate(u8* data)
{
  std::unique_ptr<BrawlbackSavestate> ss = std::make_unique<BrawlbackSavestate>();

  if (savestates.size() + 1 > MAX_ROLLBACK_FRAMES)
  {
    savestates.pop_front();
  }


  auto start = std::chrono::high_resolution_clock::now();
  ss->Capture();
  auto finish = std::chrono::high_resolution_clock::now();


  std::chrono::duration<double> elapsed = finish - start;
  INFO_LOG(BRAWLBACK, "Capture Savestate took %f\n", elapsed.count());

  savestates.push_back(std::move(ss));
}

void handleLoadSavestate(u8* data)
{

}



void handlePadData(u8* data)
{
  gfPadGamecube* pad = (gfPadGamecube*)(data);
  PADButtons buttons = pad->buttons;
  INFO_LOG(BRAWLBACK, "Is Z Held: %u\n", (pad->buttons.bits & (PADButtonBits::Z << 8)) != 0);
}

// recieve data from game into emulator
void CEXIBrawlback::DMAWrite(u32 address, u32 size)
{
  INFO_LOG(BRAWLBACK, "DMAWrite size: %u\n", size);
  u8* mem = Memory::GetPointer(address);

  if (!mem)
  {
    INFO_LOG(BRAWLBACK, "Invalid address in DMAWrite!");
    this->read_queue.clear();
    return;
  }

  u8 command_byte = mem[0];  // first byte is always cmd byte
  u8* payload = &mem[1];     // rest is payload

  switch (command_byte)
  {

    case CMD_UNKNOWN:
      INFO_LOG(BRAWLBACK, "Unknown DMAWrite command byte!");
      break;
    case CMD_ONLINE_INPUTS:
      INFO_LOG(BRAWLBACK, "DMAWrite: CMD_ONLINE_INPUTS");
      handlePadData(payload);
      break;
    case CMD_CAPTURE_SAVESTATE:
      INFO_LOG(BRAWLBACK, "DMAWrite: CMD_CAPTURE_SAVESTATE");
      handleCaptureSavestate(payload);
      break;
    case CMD_LOAD_SAVESTATE:
      INFO_LOG(BRAWLBACK, "DMAWrite: CMD_LOAD_SAVESTATE");
      break;
    default:
      INFO_LOG(BRAWLBACK, "Default DMAWrite");
      IEXIDevice::DMAWrite(address, size);
      break;
  }

}

// send data from emulator to game
void CEXIBrawlback::DMARead(u32 address, u32 size)
{
  if (!this->read_queue.empty())
  {
    INFO_LOG(BRAWLBACK, "DMARead!");
    this->read_queue.resize(size, 0);
    u8* qAddr = &this->read_queue[0];
    Memory::CopyToEmu(address, qAddr, size);
  }
  else
  {
    INFO_LOG(BRAWLBACK, "Empty DMARead queue!");
  }
  
}

bool CEXIBrawlback::IsPresent() const
{
  return true;
}
















// util

namespace Brawlback
{
  namespace Mem
  {
    std::vector<u8> uint16ToVector(u16 num)
    {
      u8 byte0 = num >> 8;
      u8 byte1 = num & 0xFF;

      return std::vector<u8>({byte0, byte1});
    }

    std::vector<u8> uint32ToVector(u32 num)
    {
      u8 byte0 = num >> 24;
      u8 byte1 = (num & 0xFF0000) >> 16;
      u8 byte2 = (num & 0xFF00) >> 8;
      u8 byte3 = num & 0xFF;

      return std::vector<u8>({byte0, byte1, byte2, byte3});
    }

    std::vector<u8> int32ToVector(int32_t num)
    {
      u8 byte0 = num >> 24;
      u8 byte1 = (num & 0xFF0000) >> 16;
      u8 byte2 = (num & 0xFF00) >> 8;
      u8 byte3 = num & 0xFF;

      return std::vector<u8>({byte0, byte1, byte2, byte3});
    }

    void appendWordToBuffer(std::vector<u8>* buf, u32 word)
    {
      auto wordVector = uint32ToVector(word);
      buf->insert(buf->end(), wordVector.begin(), wordVector.end());
    }

    void appendHalfToBuffer(std::vector<u8>* buf, u16 word)
    {
      auto halfVector = uint16ToVector(word);
      buf->insert(buf->end(), halfVector.begin(), halfVector.end());
    }

    uint8_t readByte(uint8_t* a, int& idx, uint32_t maxSize, uint8_t defaultValue)
    {
      if (idx >= (int)maxSize)
      {
        idx += 1;
        return defaultValue;
      }

      return a[idx++];
    }

    uint16_t readHalf(uint8_t* a, int& idx, uint32_t maxSize, uint16_t defaultValue)
    {
      if (idx >= (int)maxSize)
      {
        idx += 2;
        return defaultValue;
      }

      uint16_t value = a[idx] << 8 | a[idx + 1];
      idx += 2;
      return value;
    }

    uint32_t readWord(uint8_t* a, int& idx, uint32_t maxSize, uint32_t defaultValue)
    {
      if (idx >= (int)maxSize)
      {
        idx += 4;
        return defaultValue;
      }

      uint32_t value = a[idx] << 24 | a[idx + 1] << 16 | a[idx + 2] << 8 | a[idx + 3];
      idx += 4;
      return value;
    }

    float readFloat(uint8_t* a, int& idx, uint32_t maxSize, float defaultValue)
    {
      uint32_t bytes = readWord(a, idx, maxSize, *(uint32_t*)(&defaultValue));
      return *(float*)(&bytes);
    }

    const char* bit_rep[16] = {
        "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
        "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111",
    };

    void print_byte(uint8_t byte)
    {
      INFO_LOG(BRAWLBACK, "Byte: %s%s\n", bit_rep[byte >> 4], bit_rep[byte & 0x0F]);
    }

  }  // namespace Mem

}  // namespace Brawlback
