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
            Bitmap bitmap;

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
}
