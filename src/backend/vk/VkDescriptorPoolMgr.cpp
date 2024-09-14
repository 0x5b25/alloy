#include "VkDescriptorPoolMgr.hpp"

#include <vector>
#include <memory>

#include "VkCommon.hpp"

namespace Veldrid::VK::priv {

	_DescriptorPoolMgr::~_DescriptorPoolMgr(){
		////All dirty pools should be recycled by now.
		//assert(_dirtyPools.empty());
		////Retire current pool
		//_currentPool = nullptr;
//
		//while (!_freePools.empty()) {
		//	auto pool = _freePools.front();
		//	_freePools.pop();
		//	vkDestroyDescriptorPool(_dev, pool, nullptr);
		//}
	}

	void _DescriptorPoolMgr::Init(VkDevice dev, unsigned maxSets){
		assert(_currentPool == nullptr);
		_dev = dev;
		_maxSets = maxSets;
		auto firstPool = _CreatePool();
		auto firstPoolContainer = new Container();
		firstPoolContainer->mgr = this;
		firstPoolContainer->pool = firstPool;
		_currentPool.reset(firstPoolContainer);
	}

	void _DescriptorPoolMgr::DeInit(){
		//All dirty pools should be recycled by now.
		assert(_dirtyPools.empty());
		//Retire current pool
		_currentPool = nullptr;

		while (!_freePools.empty()) {
			auto pool = _freePools.front();
			_freePools.pop();
			vkDestroyDescriptorPool(_dev, pool, nullptr);
		}
	}


	void _DescriptorPoolMgr::_ReleaseContainer(Container* container){
		//std::scoped_lock l{_m_pool};

		//Mainly for debug purposes
		//assert(_dirtyPools.find(container) != _dirtyPools.end());
		//may not be in dirty pool list during manager destruction
		_dirtyPools.erase(container);

		VK_CHECK(vkResetDescriptorPool(_dev, container->pool, 0));
		_freePools.push(container->pool);
	}
	VkDescriptorPool _DescriptorPoolMgr::_GetOnePool(){
		//Should only be called from allocate
		//std::scoped_lock l{_m_pool};

		VkDescriptorPool rawPool;
		if(!_freePools.empty()){
			rawPool = _freePools.front();
			_freePools.pop();
		} else {
			rawPool = _CreatePool();
		}

		return rawPool;
	}


	VkDescriptorPool _DescriptorPoolMgr::_CreatePool()
	{
		std::vector<VkDescriptorPoolSize> sizes;
		sizes.reserve(_poolSizes.sizes.size());
		for (auto sz : _poolSizes.sizes) {
			sizes.push_back({sz.type, uint32_t(sz.multiplier * _maxSets)});
		}
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = 0;
		pool_info.maxSets = _maxSets;
		pool_info.poolSizeCount = (uint32_t)sizes.size();
		pool_info.pPoolSizes = sizes.data();

		VkDescriptorPool descriptorPool;
		VK_CHECK(vkCreateDescriptorPool(_dev, &pool_info, nullptr, &descriptorPool));

		return descriptorPool;
	}

	_DescriptorSet _DescriptorPoolMgr::Allocate(VkDescriptorSetLayout layout){

		sp<Container> toBeSwapped {nullptr};
		_DescriptorSet allocated {};

		VkDescriptorSetAllocateInfo allocInfo;
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;

		{
			//Critical section guard
			std::scoped_lock l{_m_pool};

			//Try to allocate from current pool.
			auto vkPool = _currentPool->pool;
			allocInfo.descriptorPool = vkPool;
			
			allocInfo.pSetLayouts = &layout;

			VkDescriptorSet set;
			VkResult result = vkAllocateDescriptorSets(_dev, &allocInfo, &set);

			switch (result) {
				case VK_SUCCESS: break;

				case VK_ERROR_FRAGMENTED_POOL:
				case VK_ERROR_OUT_OF_POOL_MEMORY:{
					//Fetch a new pool
					auto newPool = _GetOnePool();

					//Try to allocate from a clean pool
					allocInfo.descriptorPool = newPool;
					result = vkAllocateDescriptorSets(_dev, &allocInfo, &set);
					if(result != VK_SUCCESS){
						//Still can't allocate, maybe the set is too large?
						//Clean up
						_freePools.push(newPool);
						return allocated;
					}

					//Allocate succeeded, seems like the current pool is full
					//Add to dirty pools
					_dirtyPools.insert(_currentPool.get());
					//Wrap the raw pool
					auto container = new _DescriptorPoolMgr::Container();
					container->pool = newPool;
					container->mgr = this;
					//return sp(container);
					toBeSwapped.reset(container);
					//change current pool to new pool
					_currentPool.swap(toBeSwapped);

				} break;

				default:
					return allocated;
			}

			allocated = _DescriptorSet(
				_currentPool, set
			);
		}
		//Release the mutex to allow old container to do its clean-ups
		
		return allocated;
	}


}