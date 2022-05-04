#pragma once

#include "Common/CommonTypes.h"
#include <unordered_map>
#include "Common/ChunkFile.h"
#include "brawlback-exi-structures/ExiStructures.h"
/*
============================================
This, along with SlippiUtility.cpp is a place to hold code from the Slippi repo.

There may be things that come from Slippi in other places as well,
but this is meant to be a centralized place where large chunks of code taken from Slippi can live.

If the Slippi team feels uncomfortable with their code being used in this project,
having this code in this one file makes it easier to replace it with our own logic if the need should arise.


https://github.com/project-slippi/Ishiiruka/



A very heartfelt thank you to the Slippi team for all their hard work and dedication. <3
============================================
*/

namespace SlippiUtility
{

    namespace Savestate
    {

    // Types
    struct PreserveBlockImpl
    {
        PreserveBlock _preserveBlock;

        bool operator==(const PreserveBlockImpl& p) const
        {
          return this->_preserveBlock.address == p._preserveBlock.address &&
                 this->_preserveBlock.length == p._preserveBlock.length;
        }
    };

    typedef struct
    {
        u32 startAddress;
        u32 endAddress;
        u8* data;
        std::string regionName;
    } ssBackupLoc;

    struct preserve_hash_fn
    {
      std::size_t operator()(const PreserveBlockImpl & node) const
        {
        return node._preserveBlock.address ^ node._preserveBlock.length;  // TODO: This is probably a bad hash
        }
    };

    struct preserve_eq_fn
    {
      bool operator()(const PreserveBlock& key1, const PreserveBlock& key2) const
      {
        return key1.address == key2.address && key1.length == key2.length;
      }
    };

    typedef struct
    {
        u32 address;
        u32 value;
    } ssBackupStaticToHeapPtr;


    // Funcs
    void SlippiInitBackupLocations(std::vector<ssBackupLoc>& backupLocs,
                                    std::vector<ssBackupLoc>& fullBackupRegions,
                                    std::vector<PreserveBlockImpl>& excludeSections);

    } // namespace Savestate


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
        uint32_t readWord(uint8_t* a);
        float readFloat(uint8_t* a, int& idx, uint32_t maxSize, float defaultValue);
    }  // namespace Mem

}
