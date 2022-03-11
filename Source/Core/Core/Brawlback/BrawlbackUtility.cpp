#include "BrawlbackUtility.h"

#include "VideoCommon/OnScreenDisplay.h"


namespace Brawlback
{

    bool isButtonPressed(u16 buttonBits, PADButtonBits button)
    {
        return (buttonBits & (PADButtonBits::Z << 8)) != 0;
    }

    Match::PlayerFrameData* findInPlayerFrameDataQueue(const PlayerFrameDataQueue& queue, u32 frame) {
        for (const auto& x : queue) {
            if (x->frame == frame) {
                return x.get();
            }
        }
        return nullptr;
    }


    namespace Mem {
        
        const char* bit_rep[16] = {
            "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
            "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111",
        };

        void print_byte(uint8_t byte)
        {
            INFO_LOG(BRAWLBACK, "Byte: %s%s\n", bit_rep[byte >> 4], bit_rep[byte & 0x0F]);
        }
        void print_half(u16 half) {
            u8 byte0 = half >> 8;
            u8 byte1 = half & 0xFF;

            print_byte(byte0);
            print_byte(byte1);
        }
        void print_word(u32 word) {
            u8 byte0 = word >> 24;
            u8 byte1 = (word & 0xFF0000) >> 16;
            u8 byte2 = (word & 0xFF00) >> 8;
            u8 byte3 = word & 0xFF;

            print_byte(byte0);
            print_byte(byte1);
            print_byte(byte2);
            print_byte(byte3);
        }




        void fillByteVectorWithBuffer(std::vector<u8>& vec, u8* buf, size_t size) {
            u32 idx = 0;
            while (idx < size) {
                vec.push_back(buf[idx]);
                idx++;
            }
        }


        
    }

    namespace Sync {
        // utilities to use for logging game info & finding desyncs
        using Mem::bit_rep;

        std::string Sync::getSyncLogFilePath() { return File::GetExeDirectory() + "/synclog.txt"; }
        
        std::string Sync::str_byte(uint8_t byte)
        {
            std::string ret = std::string(bit_rep[byte >> 4]) + std::string(bit_rep[byte & 0x0F]);
            //INFO_LOG(BRAWLBACK, "Byte: %s%s\n", bit_rep[byte >> 4], bit_rep[byte & 0x0F]);
            return ret;
        }
        std::string Sync::str_half(u16 half) {
            u8 byte0 = half >> 8;
            u8 byte1 = half & 0xFF;

            std::string ret;
            ret.append(str_byte(byte0));
            ret.append(str_byte(byte1));
            return ret;
        }

        void Sync::SyncLog(const std::string& msg) {
            std::fstream synclogFile;
            File::OpenFStream(synclogFile, getSyncLogFilePath(), std::ios_base::out | std::ios_base::app);
            synclogFile << msg;
            synclogFile.close();
        }

        std::string Sync::stringifyFramedata(const Match::PlayerFrameData& pfd) {
            std::string ret;


            std::string info;
            info.append("[Frame " + std::to_string(pfd.frame) + "] [P" + std::to_string(pfd.playerIdx+1) + "]\n");


            std::string inputs;

            std::string sticks = "[StickX: " + std::to_string((int)pfd.pad.stickX) + "] [StickY: " + std::to_string((int)pfd.pad.stickY) + "]\n";
            inputs.append(sticks);
            
            std::string csticks = "[CStickX: " + std::to_string((int)pfd.pad.cStickX) + "] [CStickY: " + std::to_string((int)pfd.pad.cStickY) + "]\n";
            inputs.append(csticks);
            
            std::string triggers = "[LTrigger: " + std::to_string((int)pfd.pad.LTrigger) + "] [RTrigger: " + std::to_string((int)pfd.pad.RTrigger) + "]\n";
            inputs.append(triggers);
            
            std::string buttons = "[Buttons: " + str_half(pfd.pad.buttons) + "\n";
            inputs.append(buttons);


            ret.append(info);
            ret.append(inputs);
            return ret;
        }

    }

    namespace Dump {
        
        void DumpArray(const std::string& filename, const u8* data, size_t length)
        {
            if (!data)
                return;

            File::IOFile f(filename, "wb");

            if (!f)
            {
                ERROR_LOG(BRAWLBACK, "Failed to dump %s: Can't open file\n", filename.c_str());
                return;
            }

            if (!f.WriteBytes(data, length))
            {
                ERROR_LOG(BRAWLBACK, "Failed to dump %s: Failed to write to file\n", filename.c_str());
            }
        }

        void DoMemDumpIteration(int& dump_num) {
            std::string dump_num_str = std::to_string(dump_num);
            std::string frame_folder = File::GetUserPath(D_DUMP_IDX) + "/memdumps/dump" + dump_num_str;
            if (!std::filesystem::exists(frame_folder))
                std::filesystem::create_directories(frame_folder);
            
            std::string mem1_file = frame_folder + "/mem1_" + dump_num_str + ".raw";
            std::string mem2_file = frame_folder + "/mem2_" + dump_num_str + ".raw";

            Dump::DumpMem(AddressSpace::Type::Mem1, mem1_file);
            Dump::DumpMem(AddressSpace::Type::Mem2, mem2_file);
            dump_num += 1;
        }

        void DumpMem(AddressSpace::Type memType, const std::string& dumpPath) {
            AddressSpace::Accessors* accessors = AddressSpace::GetAccessors(memType);
            DumpArray(dumpPath, accessors->begin(),
                    std::distance(accessors->begin(), accessors->end()));
        }
    }

}
