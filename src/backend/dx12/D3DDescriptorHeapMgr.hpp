#pragma once

//3rd-party headers

//veldrid public headers
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/BindableResource.hpp"
#include "veldrid/Pipeline.hpp"
#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SyncObjects.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"

//standard library headers
#include <vector>
#include <queue>
#include <unordered_set>
#include <mutex>

//backend specific headers
#include "DXCTexture.hpp"

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers

namespace Veldrid::DXC {

    class _DescriptorSet;

    class _DescriptorHeapMgr{

    public:
        struct Container : public RefCntBase{
            ID3D12DescriptorHeap* pool;
            _DescriptorHeapMgr* mgr;
            uint32_t nextFreeSlot;

            Container (
                ID3D12DescriptorHeap* pool,
                _DescriptorHeapMgr* mgr
            ) : pool(pool)
              , mgr(mgr)
              , nextFreeSlot(0)
            {}

            ~Container(){
                mgr->_ReleaseContainer(this);
            }
        };
    
    private:
        ID3D12Device* _dev;
    
        uint32_t _maxDescCnt;
        D3D12_DESCRIPTOR_HEAP_TYPE _type;
        uint32_t _incrSize;
        //PoolSizes _poolSizes;
    
        //'Clean' pools.
        std::queue<ID3D12DescriptorHeap*> _freePools;
        //previously full pools, some sets might be freed, but at least one set is in use.
        std::unordered_set<Container*> _dirtyPools;
        //Currently active pool, that is not full.
        sp<Container> _currentPool;
    
        std::mutex _m_pool;
    
        //Thread-safe release
        void _ReleaseContainer(Container* container);
    
        ID3D12DescriptorHeap* _GetOnePool();
        ID3D12DescriptorHeap* _CreatePool(uint32_t maxDescCnt);
    //
    public:
        _DescriptorHeapMgr(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxDescCnt);
        ~_DescriptorHeapMgr();
    //
    //	void Init(VkDevice dev, unsigned maxSets);
    //	void DeInit();
    //
        _DescriptorSet Allocate(const std::vector<sp<TextureView>>& res);
    };



    class _DescriptorSet{
        DISABLE_COPY_AND_ASSIGN(_DescriptorSet);
    
        sp<_DescriptorHeapMgr::Container> _pool;
        D3D12_CPU_DESCRIPTOR_HANDLE  _descSetStart;
        uint32_t _descCount;
    
    public:
        _DescriptorSet()
            : _pool{nullptr}
            , _descSetStart{}
            , _descCount(0)
        {}
    
        _DescriptorSet(
            const sp<_DescriptorHeapMgr::Container>& pool,
            const D3D12_CPU_DESCRIPTOR_HANDLE& descSetStart,
            uint32_t descCount
        )
            : _pool(pool)
            , _descSetStart(descSetStart)
            , _descCount(descCount)
        {}
    
        ~_DescriptorSet(){
        //No need to vkDestroy the desc set
        //since the whole will be reseted once
        //there is no reference to the pool.
        }
    
        //move semantics support
        _DescriptorSet(_DescriptorSet&& that)
            : _pool(std::move(that._pool))
            , _descSetStart(that._descSetStart)
            , _descCount(that._descCount)
        {}
    
        _DescriptorSet& operator=(_DescriptorSet&& that){
            _pool = std::move(that._pool);
            _descSetStart = that._descSetStart;
            _descCount = that._descCount;
            return *this;
        }
    
        const D3D12_CPU_DESCRIPTOR_HANDLE& GetHandle() const {return _descSetStart;}
        uint32_t GetCount() const {return _descCount;}
    
    };

}
