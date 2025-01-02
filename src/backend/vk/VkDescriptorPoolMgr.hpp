#pragma once

#include <volk.h>

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/common/Macros.h"

#include <cstdint>
#include <queue>
#include <unordered_set>
#include <mutex>

namespace Veldrid::VK::priv {

	class _DescriptorSet;

	struct DescriptorResourceCounts
    {
        std::uint32_t uniformBufferCount;
        std::uint32_t sampledImageCount;
        std::uint32_t samplerCount;
        std::uint32_t storageBufferCount;
        std::uint32_t storageImageCount;
        std::uint32_t uniformBufferDynamicCount;
        std::uint32_t storageBufferDynamicCount;
    };

	struct PoolSize {
		VkDescriptorType type;
		float multiplier;
	};
	struct PoolSizes {
		std::vector<PoolSize> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1.f },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1.f }
		};
	};

	class _DescriptorPoolMgr{

	public:
		 struct Container : public RefCntBase{
			VkDescriptorPool pool;
			_DescriptorPoolMgr* mgr;

			~Container(){
				mgr->_ReleaseContainer(this);
			}
		 };

	private:
		VkDevice _dev;

		unsigned _maxSets;
		PoolSizes _poolSizes;

		//'Clean' pools.
		std::queue<VkDescriptorPool> _freePools;
		//previously full pools, some sets might be freed, but at least one set is in use.
		std::unordered_set<Container*> _dirtyPools;
		//Currently active pool, that is not full.
		sp<Container> _currentPool;

		std::mutex _m_pool;

		//Thread-safe release
		void _ReleaseContainer(Container* container);

		VkDescriptorPool _GetOnePool();
		VkDescriptorPool _CreatePool();

	public:
		//Must call Init
		_DescriptorPoolMgr(){}
		~_DescriptorPoolMgr();

		void Init(VkDevice dev, unsigned maxSets);
		void DeInit();

		_DescriptorSet Allocate(VkDescriptorSetLayout layout);
	};



	class _DescriptorSet{
		DISABLE_COPY_AND_ASSIGN(_DescriptorSet);

		sp<_DescriptorPoolMgr::Container> _pool;
		VkDescriptorSet _descSet;

	public:
		_DescriptorSet()
			: _pool{nullptr}
			, _descSet{VK_NULL_HANDLE}
		{}

		_DescriptorSet(
			const sp<_DescriptorPoolMgr::Container>& pool,
			VkDescriptorSet set	
		)
			: _pool(pool)
			, _descSet(set)
		{}


		~_DescriptorSet(){
			//No need to vkDestroy the desc set
			//since the whole will be reseted once
			//there is no reference to the pool.
		}

		//move semantics support
		_DescriptorSet(_DescriptorSet&& another)
			: _pool(std::move(another._pool))
			, _descSet(another._descSet)
		{
			another._descSet = VK_NULL_HANDLE;
		}
		
		_DescriptorSet& operator=(_DescriptorSet&& that){
		 	_pool = std::move(that._pool);
			_descSet = that._descSet;
			return *this;
		}

		const VkDescriptorSet& GetHandle() const {return _descSet;}

	};
}