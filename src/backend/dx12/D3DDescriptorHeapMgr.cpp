#include "D3DDescriptorHeapMgr.hpp"

#include "alloy/common/Common.hpp"

//#include "DXCDevice.hpp"

#include <bit>

namespace alloy::dxc {

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

    _DescriptorHeapMgr::_DescriptorHeapMgr(
        ID3D12Device* dev,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t maxDescCnt
    )
        : _dev(dev)
        , _type(type)
        , _maxDescCnt(maxDescCnt)
    {
        assert(dev != nullptr);
        _incrSize = _dev->GetDescriptorHandleIncrementSize(type);
    }

    _DescriptorHeapMgr::~_DescriptorHeapMgr(){
        for (auto& pool : _pools) {
            pool.heap->Release();
        }
    }

    ID3D12DescriptorHeap* _DescriptorHeapMgr::_CreatePool(uint32_t maxDescCnt)
    {
        ID3D12DescriptorHeap* pHeap = nullptr;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        heapDesc.NumDescriptors = maxDescCnt;
        heapDesc.Type = _type;
        auto hr = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pHeap));
        if (FAILED(hr))
        {
            //Running = false;
        }

        return pHeap;
    }

    _Descriptor _DescriptorHeapMgr::Allocate() {
        
        _Descriptor result {};
        {
            //Critical section guard
            std::scoped_lock l{_m_pool};


            uint32_t i = 0;
            for(; i < _pools.size(); i++) {
                auto& pool = _pools[i];

                uint32_t slotIdx = 0;
                if(!pool.bitmap.Find(slotIdx)) {
                    continue;
                }

                //Try to allocate from current pool.
                result.handle = pool.heap->GetCPUDescriptorHandleForHeapStart();
                result.handle.ptr += _incrSize * slotIdx;
                result.poolIndex = i;
            }
            
            if(!result) {
                auto newHeap = _CreatePool(_maxDescCnt);
                auto& newPool = _pools.emplace_back(newHeap, _maxDescCnt);
                newPool.bitmap.Set(0);

                result.handle = newPool.heap->GetCPUDescriptorHandleForHeapStart();
                result.poolIndex = i;
            }

        }
    
        return result;
    }

    
    void _DescriptorHeapMgr::Free(const _Descriptor& desc) {
        assert(desc);
        assert(desc.poolIndex < _pools.size());
        {
            //Critical section guard
            std::scoped_lock l{_m_pool};

            auto& pool = _pools[desc.poolIndex];

            auto hHeapStart = pool.heap->GetCPUDescriptorHandleForHeapStart();
            auto delta = (size_t)desc.handle.ptr - (size_t)hHeapStart.ptr;
            auto slotIdx = delta / _incrSize;
            assert(delta % _incrSize == 0);
            assert(slotIdx < _maxDescCnt);

            pool.bitmap.Clear(slotIdx);
        }
    }
}
