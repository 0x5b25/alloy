#include "BumpAllocator.hpp"


BumpAllocator::BumpAllocator(alloy_sp<alloy::IGraphicsDevice> dev)
    : _dev(dev)
    , _currFrameIdx(0)
    , _lastCompletedFrameIdx(0)
{ }

BumpAllocator::~BumpAllocator() {
    //TODO: how to handle unfreed allocations?
}


void BumpAllocator::_FetchOrAllocateChunk(uint32_t sizeInBytes) {
    alloy_sp<alloy::IBuffer> buffer;
    if(sizeInBytes <= chunkSize) {
        //We can get a standard allocated buffer
        auto& q = _circularBuffer.freeChunks;
        if(!q.empty()) {
            buffer = q.front();
            q.pop();
        }
    }

    if(!buffer) {
        //need to allocate a new buffer
        buffer = _AllocateBuffer(sizeInBytes);
    }

    _circularBuffer.chunks.push({buffer, 0});
    _circularBuffer.usedBytesInCurrentChunk = 0;
}

alloy::common::sp<alloy::IBuffer> BumpAllocator::_AllocateBuffer(uint32_t sizeInBytes) {
    auto& factory = _dev->GetResourceFactory();
    alloy::IBuffer::Description desc{};

    desc.usage.vertexBuffer = 1;
    desc.usage.indexBuffer = 1;
    desc.usage.uniformBuffer = 1;
    desc.usage.structuredBufferReadWrite = 1;

    desc.sizeInBytes = std::max(sizeInBytes, chunkSize);
    desc.hostAccess = alloy::HostAccess::PreferDeviceMemory;

    return factory.CreateBuffer(desc);
}

alloy::common::sp<alloy::BufferRange> BumpAllocator::Allocate(
    uint32_t sizeInBytes, 
    uint32_t alignmentInBytes
) {
    if(_circularBuffer.chunks.empty()) {
        _FetchOrAllocateChunk(sizeInBytes);
    }
    //Do we have enough size in current buffer?
    auto* pCurrChunk = &_circularBuffer.chunks.back();
    uint32_t currChunkSize = pCurrChunk->buffer->GetDesc().sizeInBytes;
    uint32_t currChunkTop = _circularBuffer.usedBytesInCurrentChunk;

    uint32_t alignedUnits = (currChunkTop + (alignmentInBytes - 1)) / alignmentInBytes;
    uint32_t alignedTop = alignedUnits * alignmentInBytes;

    int sizeLeft = currChunkSize - alignedTop;
    if(sizeLeft < sizeInBytes) {
        //Not enough size. Fetch a new chunk
        _FetchOrAllocateChunk(sizeInBytes);
        pCurrChunk = &_circularBuffer.chunks.back();
        alignedTop = 0;
    }

    //All checks passed.
    //Bump used size
    _circularBuffer.usedBytesInCurrentChunk = alignedTop + sizeInBytes;
    //Bump frame index
    pCurrChunk->latestFrameIdx = _currFrameIdx;

    //Return results
    return alloy::BufferRange::MakeByteBuffer(pCurrChunk->buffer, alignedTop, sizeInBytes);
}


void BumpAllocator::BeginNewFrame() {
    _currFrameIdx++;
}

void BumpAllocator::NotifyFrameComplete() {
    _lastCompletedFrameIdx++;
    //Recycle all chunks not in use
    auto& q = _circularBuffer.chunks;
    while(!q.empty()) {
        if(q.front().latestFrameIdx > _lastCompletedFrameIdx) {
            auto buffer = q.front().buffer;
            q.pop();
            //Recycle if this is a standard allocatec chunk
            auto bufferSize = buffer->GetDesc().sizeInBytes;
            if(bufferSize == chunkSize) {
                _circularBuffer.freeChunks.push(buffer);
            }
        } else {
            break;
        }
    }

    if(q.empty()) {
        _circularBuffer.usedBytesInCurrentChunk = 0;
    }
}

