

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
  auto start = std::chrono::high_resolution_clock::now();
  ss->Capture();
  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;
  INFO_LOG(BRAWLBACK, "Capture Savestate took %f\n", elapsed.count());
  savestateTest = std::move(ss);
  /*
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
  */
  
}

void CEXIBrawlback::handleLoadSavestate(u8* data)
{

  /*
  if (!savestates.empty())
  {
    auto start = std::chrono::high_resolution_clock::now();
    savestates.front()->Load({});
    auto finish = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = finish - start;
    INFO_LOG(BRAWLBACK, "Load Savestate took %f\n", elapsed.count());
  }
  else
  {
    INFO_LOG(BRAWLBACK, "Empty savestate queue when trying to load state!");
  }
  */

  if (this->savestateTest)
  {
    auto start = std::chrono::high_resolution_clock::now();
    this->savestateTest->Load({});
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    INFO_LOG(BRAWLBACK, "Load Savestate took %f\n", elapsed.count());
  }
  

 
}



void handlePadData(u8* data)
{
  gfPadGamecube* pad = (gfPadGamecube*)(data);
  PADButtons buttons = pad->buttons;
  INFO_LOG(BRAWLBACK, "Is Z Held: %u\n", (pad->buttons.bits & PADButtonBits::Z) != 0);

  free(data);
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
      handleLoadSavestate(payload);
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
