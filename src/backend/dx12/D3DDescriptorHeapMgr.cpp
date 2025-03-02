#include "D3DDescriptorHeapMgr.hpp"

#include "alloy/common/Common.hpp"

//#include "DXCDevice.hpp"

#include <bit>

namespace alloy::dxc {

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

    _ShaderResDescriptorHeapMgr::_ShaderResDescriptorHeapMgr(
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

    _ShaderResDescriptorHeapMgr::~_ShaderResDescriptorHeapMgr() {
        for (auto& pool : _pools) {
            pool.heap->Release();
        }
    }

    ID3D12DescriptorHeap* _ShaderResDescriptorHeapMgr::_CreatePool(uint32_t maxDescCnt)
    {
        ID3D12DescriptorHeap* pHeap = nullptr;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = maxDescCnt;
        heapDesc.Type = _type;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Shader visible for GPU access
        auto hr = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pHeap));
        if (FAILED(hr))
        {
            //Running = false;
        }

        return pHeap;
    }

    _ShaderResDescriptor _ShaderResDescriptorHeapMgr::Allocate(uint32_t count) {
        _ShaderResDescriptor result{};
        {
            // Critical section guard
            std::scoped_lock l{_m_pool};

            uint32_t i = 0;
            if (count <= _maxDescCnt) {
                for (; i < _pools.size(); i++) {
                    auto& pool = _pools[i];

                    // Try to allocate from current pool using FreeListAllocator
                    auto pAlloc = pool.allocator.Allocate(count);
                    if (!pAlloc) {
                        continue;
                    }

                    // Successfully allocated from this pool
                    result.handle = pool.heap->GetGPUDescriptorHandleForHeapStart();
                    result.handle.ptr += _incrSize * pAlloc->offset;
                    result.count = count;
                    result.handleIncrSize = _incrSize;
                    result.poolIndex = i;
                    result.pAlloc = pAlloc;
                    break;
                }
            } else {
                i = (uint32_t)_pools.size();
            }
            
            // If no allocation was successful, create a new pool
            if (!result) {
                auto newSize = std::max(count, (uint32_t)_maxDescCnt);
                auto newHeap = _CreatePool(newSize);
                auto& newPool = _pools.emplace_back(newHeap, newSize);
                
                auto pAlloc = newPool.allocator.Allocate(count);
                assert(pAlloc); // Should always succeed with a new pool
                
                result.handle = newPool.heap->GetGPUDescriptorHandleForHeapStart();
                result.handle.ptr += _incrSize * pAlloc->offset;
                result.count = count;
                result.handleIncrSize = _incrSize;
                result.poolIndex = i;
                result.pAlloc = pAlloc;
            }
        }
    
        return result;
    }

    void _ShaderResDescriptorHeapMgr::Free(const _ShaderResDescriptor& desc) {
        assert(desc);
        assert(desc.pAlloc);
        assert(desc.poolIndex < _pools.size());
        
        {
            // Critical section guard
            std::scoped_lock l{_m_pool};
            
            // Use the poolIndex to directly access the correct pool
            auto& pool = _pools[desc.poolIndex];
            
            // Free the allocation in the FreeListAllocator
            pool.allocator.Free(desc.pAlloc);
        }
    }
}
