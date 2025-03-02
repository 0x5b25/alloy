#include "Allocators.hpp"
#include <algorithm>
#include <limits>
#include <cassert>
#include <bit>

namespace alloy::utils {
    
    FreeListAllocator::FreeListAllocator(uint64_t size)
        : _size(size), _head(nullptr), _maxFreeBlockSize(size)
    {
        // Initialize with a single free block that spans the entire size
        _head = new Block{ 0, size, true, nullptr };
    }

    FreeListAllocator::~FreeListAllocator()
    {
        // Clean up all blocks in the linked list
        Block* current = _head;
        while (current != nullptr)
        {
            Block* next = current->pNext;
            delete current;
            current = next;
        }
        _head = nullptr;
    }

    // Helper function to update the maximum free block size
    void FreeListAllocator::_UpdateMaxFreeBlockSize()
    {
        _maxFreeBlockSize = 0;
        Block* current = _head;
        
        while (current != nullptr)
        {
            if (current->isFree && current->size > _maxFreeBlockSize)
            {
                _maxFreeBlockSize = current->size;
            }
            current = current->pNext;
        }
    }

    FreeListAllocator::Block* FreeListAllocator::Allocate(uint64_t size)
    {
        if (size == 0)
            return nullptr;

        // Use best-fit strategy to find the smallest free block that can accommodate the requested size
        Block* bestFit = nullptr;
        uint64_t minWastedSpace = std::numeric_limits<uint64_t>::max();
        
        Block* current = _head;
        while (current != nullptr)
        {
            if (current->isFree && current->size >= size)
            {
                uint64_t wastedSpace = current->size - size;
                if (wastedSpace < minWastedSpace)
                {
                    bestFit = current;
                    minWastedSpace = wastedSpace;
                    
                    // Perfect fit, no need to search further
                    if (wastedSpace == 0)
                        break;
                }
            }
            current = current->pNext;
        }
        
        // No suitable block found
        if (bestFit == nullptr)
            return nullptr;
            
        // If the block is significantly larger than requested, split it
        if (bestFit->size > size && (bestFit->size - size) > sizeof(Block))
        {
            // Create a new block after the allocated portion
            Block* newBlock = new Block{
                bestFit->offset + size,  // Offset starts after the allocated portion
                bestFit->size - size,    // Size is the remaining space
                true,                    // The new block is free
                bestFit->pNext           // It links to the same next block
            };
            
            // Update the current block
            bestFit->size = size;
            bestFit->pNext = newBlock;
        }
        
        // Mark the block as used
        bestFit->isFree = false;
        
        // Update max free block size after allocation
        _UpdateMaxFreeBlockSize();
        
        return bestFit;
    }

    void FreeListAllocator::Free(Block* block)
    {
        assert(block != nullptr);
        assert(block->isFree == false);
        
        // Find the block's position in the list to determine merge possibilities
        Block* prev = nullptr;
        Block* current = _head;
        
        // Find the block and its previous block
        while (current != nullptr && current != block)
        {
            prev = current;
            current = current->pNext;
        }
        
        // If block was not found in the list, return
        if (current == nullptr)
            return;

        // Mark the block as free
        block->isFree = true;
        
        // Case 1: Can merge with next block
        if (current->pNext != nullptr && current->pNext->isFree)
        {
            // Merge with the next block
            current->size += current->pNext->size;
            current->pNext = current->pNext->pNext;
            delete current->pNext;
        }
        
        // Case 2: Can merge with previous block
        if (prev != nullptr && prev->isFree)
        {
            // Merge the previous block with the current block
            prev->size += current->size;
            prev->pNext = current->pNext;
            delete current;
        }
        
        // Case 3: Cannot merge with any adjacent blocks
        // Nothing to do here as the block is already marked as free
        
        // Update max free block size after freeing
        _UpdateMaxFreeBlockSize();
    }

    uint64_t FreeListAllocator::GetMaxFreeBlockSize() const
    {
        // Simply return the cached value
        return _maxFreeBlockSize;
    }

    
    Bitmap::Bitmap(uint32_t bitCnt)
        : bitCnt(bitCnt)
        , depth(0)
    {
        uint32_t wordCnt = 1;
        while ((1ull << ((depth + 1) * 6)) < bitCnt) {
            depth++;
            wordCnt += wordCnt * 64;
        }

        payload = new uint64_t[wordCnt];
        for (uint32_t i = 0; i < wordCnt; i++) {
            payload[i] = 0;
        }
    }

    void Bitmap::ModifyBit(uint32_t whichBit, bool value) {

        auto _RecrBody = [this, whichBit, value](
            uint64_t* layerBase,
            uint32_t currDepth,
            auto& _RecrBodyRef
        ) {
            auto layerSizeInWord = 1u << (currDepth * 6);
            auto layerBitCoveragePerWordLog2 = (depth - currDepth + 1) * 6;
            auto idx = whichBit >> layerBitCoveragePerWordLog2;
            auto whichBitInWord = (whichBit >> (layerBitCoveragePerWordLog2 - 6)) & 0x3f;
            auto& whichWord = layerBase[idx];
            auto mask = 1ull << whichBitInWord;

            if (currDepth == depth) {
                if (value) {
                    whichWord |= mask;
                    return whichWord == 0xffffffff'ffffffff;
                }
                else {
                    whichWord &= ~mask;
                    return false;
                }
            }
            else {
                auto nextLayerBase = layerBase + layerSizeInWord;
                bool isChildAllSet
                    = _RecrBodyRef(nextLayerBase, currDepth + 1, _RecrBodyRef);
                if (value) {
                    if (isChildAllSet) {
                        whichWord |= mask;
                    }
                    return whichWord == 0xffffffff'ffffffff;
                }
                else {
                    whichWord &= ~mask;
                    return false;
                }
            }
        };

        _RecrBody(payload, 0, _RecrBody);
    }

    bool Bitmap::Find(uint32_t& whichBit) const {
        uint32_t currDepth = 0;
        uint32_t currChunkIdx = 0;
        uint64_t* pLayer = payload;
        while (currDepth <= depth) {
            auto index = pLayer[currChunkIdx];
            //Find the first "0" in index
            auto freeChildChunk = std::countr_zero(~index);
            if (freeChildChunk >= 64)
                return false;
            currChunkIdx = (currChunkIdx << 6) + freeChildChunk;

            auto layerSizeInWord = 1u << (currDepth * 6);
            pLayer += layerSizeInWord;
            currDepth++;
        }
        whichBit = currChunkIdx;
        return whichBit < bitCnt;
    }
}