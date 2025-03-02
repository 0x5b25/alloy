#pragma once

#include <cassert>
#include <cstdint>

namespace alloy::utils
{

    class FreeListAllocator
    {

    public:
        struct Block {
            uint64_t offset;
            uint64_t size;
            bool isFree;
            Block* pNext;
        };

        using Allocation = Block;

    private:
        Block* _head;
        uint64_t _size;
        uint64_t _maxFreeBlockSize; // Store the maximum free block size
        
        // Helper method to update the maximum free block size
        void _UpdateMaxFreeBlockSize();

    public:
        FreeListAllocator(uint64_t size);
        ~FreeListAllocator();

        Block* Allocate(uint64_t size);
        void Free(Block* block);

        uint64_t GetMaxFreeBlockSize() const;
    };

    
    /*
    A 3-level tree structure:

    +-----------+------------------------------------------------
    |           | +----------------------    ------------+ +------------------------+
    |  flag2-0  | | flag1-0 | flag1-1 |  ...  | flag1-63 | | flag0-0 ... flag0-4095 |
    |           | +----------------------    ------------+ +------------------------+
    +-----------+------------------------------------------------
    
    */
    class Bitmap {
        uint32_t bitCnt;
        uint32_t depth;
        uint64_t* payload;

        void ModifyBit(uint32_t whichBit, bool value);

    public:

        Bitmap(uint32_t bitCnt);

        ~Bitmap() {
            delete[] payload;
        }

        bool Test(uint32_t whichBit) const {
            assert(whichBit < bitCnt);
            uint32_t currDepth = 0;
            uint64_t* pLayer = payload;
            while(currDepth < depth) {
                auto layerSizeInWord = 1u << ( currDepth * 6 );
                pLayer += layerSizeInWord;
            }
            auto& whichWord = pLayer[whichBit >> 6];
            auto mask = 1 << (whichBit & 0x3f);
            return whichWord & mask;
        }

        void Set(uint32_t whichBit) { 
            assert(whichBit < bitCnt);
            ModifyBit(whichBit, 1);
        }
        void Clear(uint32_t whichBit) {
            assert(whichBit < bitCnt);
            ModifyBit(whichBit, 0);
        }

        void ClearAll() {
            uint32_t currDepth = 0;
            uint64_t* pLayer = payload;
            while (currDepth <= depth) {
                auto layerSizeInWord = 1u << (currDepth * 6);
                for (uint32_t i = 0; i < layerSizeInWord; i++) {
                    pLayer[i] = 0;
                }
                pLayer += layerSizeInWord;
                currDepth++;
            }
        }

        void SetAll() {
            uint32_t currDepth = 0;
            uint64_t* pLayer = payload;
            while (currDepth <= depth) {
                auto layerSizeInWord = 1u << (currDepth * 6);
                for (uint32_t i = 0; i < layerSizeInWord; i++) {
                    pLayer[i] = 0xffffffff'ffffffff;
                }
                pLayer += layerSizeInWord;
                currDepth++;
            }
        }

        bool IsFull() const {
            return *payload == 0xffffffff'ffffffff;
        }

        bool Find(uint32_t& whichBit) const;
    };

}