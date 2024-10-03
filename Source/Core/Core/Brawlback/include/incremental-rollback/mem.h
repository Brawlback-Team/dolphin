#pragma once
#include "util.h"


#define rbMemcpy(dst, src, size) fastMemcpy(dst, src, size)

struct Buffer
{
    char* data = nullptr;
    u64 size = 0;
    bool operator==(const Buffer& buf) const { return data == buf.data && size == buf.size; }
};

struct AddressArray
{
    void** Addresses;
    u64 Count;
};

struct TrackedBuffer
{
    Buffer buffer; // the actual buffer this is tracking
    AddressArray changedPages; // the pages that have been written to
};

extern std::vector<TrackedBuffer> TrackedMemList;
extern std::vector<Buffer> ExcludeMemList;
    // passed in memory block MUST be allocated (at least on windows...) with VirtualAlloc and the MEM_WRITE_WATCH flag
void TrackAlloc(void* ptr, size_t size);
void ExcludeMem(void* ptr, size_t size);
void UntrackAlloc(void* ptr);
void ResetAllocs();
void PrintTrackedBuf(const TrackedBuffer& buf);
void ResetWrittenPages();
int GetWrittenPages(char* base, u64 baseSize, void** writtenToPages, u64& pageCount);
bool GetAndResetWrittenPages(void** changedPageAddresses, u64* numChangedPages, u64 maxEntries);
void fastMemcpy(void* pvDest, void* pvSrc, size_t nBytes);
