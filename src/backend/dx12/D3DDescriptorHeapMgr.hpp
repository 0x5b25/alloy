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

//backend specific headers

//platform specific headers
#include <directx/d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers

namespace Veldrid::DXC::priv {

    class _DescriptorSet;

    //class _DescriptorHeapMgr{
    //
    //public:
    //	 struct Container : public RefCntBase{
    //	VkDescriptorPool pool;
    //	_DescriptorHeapMgr* mgr;
    //
    //	~Container(){
    //	mgr->_ReleaseContainer(this);
    //	}
    //	 };
    //
    //private:
    //	VkDevice _dev;
    //
    //	unsigned _maxSets;
    //	PoolSizes _poolSizes;
    //
    //	//'Clean' pools.
    //	std::queue<VkDescriptorPool> _freePools;
    //	//previously full pools, some sets might be freed, but at least one set is in use.
    //	std::unordered_set<Container*> _dirtyPools;
    //	//Currently active pool, that is not full.
    //	sp<Container> _currentPool;
    //
    //	std::mutex _m_pool;
    //
    //	//Thread-safe release
    //	void _ReleaseContainer(Container* container);
    //
    //	VkDescriptorPool _GetOnePool();
    //	VkDescriptorPool _CreatePool();
    //
    //public:
    //	//Must call Init
    //	_DescriptorPoolMgr(){}
    //	~_DescriptorPoolMgr();
    //
    //	void Init(VkDevice dev, unsigned maxSets);
    //	void DeInit();
    //
    //	_DescriptorSet Allocate(VkDescriptorSetLayout layout);
    //};
    //
    //
    //
    //class _DescriptorSet{
    //	DISABLE_COPY_AND_ASSIGN(_DescriptorSet);
    //
    //	sp<_DescriptorPoolMgr::Container> _pool;
    //	VkDescriptorSet _descSet;
    //
    //public:
    //	_DescriptorSet()
    //	: _pool{nullptr}
    //	, _descSet{VK_NULL_HANDLE}
    //	{}
    //
    //	_DescriptorSet(
    //	const sp<_DescriptorPoolMgr::Container>& pool,
    //	VkDescriptorSet set	
    //	)
    //	: _pool(pool)
    //	, _descSet(set)
    //	{}
    //
    //	~_DescriptorSet(){
    //	//No need to vkDestroy the desc set
    //	//since the whole will be reseted once
    //	//there is no reference to the pool.
    //	}
    //
    //	//move semantics support
    //	_DescriptorSet(_DescriptorSet&& that)
    //	: _pool(std::move(that._pool))
    //	, _descSet(that._descSet)
    //	{}
    //
    //	_DescriptorSet& operator=(_DescriptorSet&& that){
    //	 	_pool = std::move(that._pool);
    //	_descSet = that._descSet;
    //	return *this;
    //	}
    //
    //	const VkDescriptorSet& GetHandle() const {return _descSet;}
    //
    //};

}
