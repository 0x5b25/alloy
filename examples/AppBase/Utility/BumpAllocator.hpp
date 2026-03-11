#pragma once

#include <queue>
#include <alloy/alloy.hpp>

//Simple allocator aim to handle per-frame temp data
class BumpAllocator {
    template<typename T> using alloy_sp = alloy::common::sp<T>;
public:
    static constexpr uint32_t chunkSize = 0x100'0000; // 16 MB per chunk
    static constexpr uint32_t chunkAlignment = 0x1000;


private:
    alloy_sp<alloy::IGraphicsDevice> _dev;

    struct CircularBuffer {
        struct Chunk {
            alloy_sp<alloy::IBuffer> buffer;
            uint32_t latestFrameIdx;
        };

        std::queue<Chunk> chunks;
        uint32_t usedBytesInCurrentChunk;
        std::queue<alloy_sp<alloy::IBuffer>> freeChunks;
    } _circularBuffer;

    uint32_t _currFrameIdx, _lastCompletedFrameIdx;

    void _FetchOrAllocateChunk(uint32_t sizeInBytes);
    alloy_sp<alloy::IBuffer> _AllocateBuffer(uint32_t sizeInBytes);

public:

    BumpAllocator(alloy_sp<alloy::IGraphicsDevice> dev);
    ~BumpAllocator();

    //Threre must be a BeginNewFrame call before any allocations
    //can be made
    void BeginNewFrame();
    void NotifyFrameComplete();

    alloy_sp<alloy::BufferRange> Allocate(uint32_t sizeInBytes, uint32_t alignmentInBytes);
};
