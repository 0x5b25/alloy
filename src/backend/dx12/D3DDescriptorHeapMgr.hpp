#pragma once

//3rd-party headers

//alloy public headers
//#include "alloy/common/RefCnt.hpp"
//
//#include "alloy/BindableResource.hpp"
//#include "alloy/Pipeline.hpp"
//#include "alloy/GraphicsDevice.hpp"
//#include "alloy/SyncObjects.hpp"
//#include "alloy/Buffer.hpp"
//#include "alloy/SwapChain.hpp"
#include "utils/Allocators.hpp"

//standard library headers
#include <vector>
#include <mutex>
#include <cassert>

//backend specific headers
//#include "DXCTexture.hpp"

//platform specific headers
#include <d3d12.h>
//#include <dxgi1_4.h> //Guaranteed by DX12

//Local headers

namespace alloy::dxc {

    struct _Descriptor{
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        uint32_t poolIndex;

        operator bool() const {
            return handle.ptr;
        }
    };

    class _DescriptorHeapMgr{

    public:
        struct Container {
            ID3D12DescriptorHeap* heap;
            alloy::utils::Bitmap bitmap;

            Container (
                ID3D12DescriptorHeap* heap,
                uint32_t descCount
            ) : heap(heap)
              , bitmap(descCount)
            {}
        };
    
    private:
        ID3D12Device* _dev;
    
        uint32_t _maxDescCnt;
        D3D12_DESCRIPTOR_HEAP_TYPE _type;
        uint32_t _incrSize;
        //PoolSizes _poolSizes;
    
        std::vector<Container> _pools;
    
        std::mutex _m_pool;
    
        ID3D12DescriptorHeap* _CreatePool(uint32_t maxDescCnt);
    //
    public:
        _DescriptorHeapMgr(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxDescCnt);
        ~_DescriptorHeapMgr();
    //
    //	void Init(VkDevice dev, unsigned maxSets);
    //	void DeInit();
    //
        _Descriptor Allocate();
        void Free(const _Descriptor&);
    };

    struct _ShaderResDescriptor{
        D3D12_GPU_DESCRIPTOR_HANDLE handle;
        uint32_t count;
        uint32_t handleIncrSize;
        uint32_t poolIndex;
        
        alloy::utils::FreeListAllocator::Allocation* pAlloc;

        operator bool() const {
            return handle.ptr;
        }
    };
    
    class _ShaderResDescriptorHeapMgr{

        struct Container {
            ID3D12DescriptorHeap* heap;
            alloy::utils::FreeListAllocator allocator;

            Container (
                ID3D12DescriptorHeap* heap,
                uint32_t descCount
            ) : heap(heap)
              , allocator(descCount)
            {}
        };

        ID3D12Device* _dev;
    
        uint32_t _maxDescCnt;
        D3D12_DESCRIPTOR_HEAP_TYPE _type;
        uint32_t _incrSize;
        //PoolSizes _poolSizes;
    
        std::vector<Container> _pools;
    
        std::mutex _m_pool;
    
        ID3D12DescriptorHeap* _CreatePool(uint32_t maxDescCnt);


    public:
        _ShaderResDescriptorHeapMgr(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxDescCnt);
        ~_ShaderResDescriptorHeapMgr();

        _ShaderResDescriptor Allocate(uint32_t count);
        void Free(const _ShaderResDescriptor&);

    };
}
