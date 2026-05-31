#include "VkDescriptorPoolMgr.hpp"

#include <vector>
#include <memory>

#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanBindableResource.hpp"

namespace alloy::vk {

    
    _DescriptorPoolMgr::_DescriptorPoolMgr(const CreateArgs& args)
        : _dev (args.dev)
        , _poolType (args.type)
        , _maxSetsPerPool (args.maxSetsPerPool)
        , _maxDescriptorsPerPool (args.maxDescriptorsPerPool)
        , _currentPool(nullptr)
    { }

    _DescriptorPoolMgr::~_DescriptorPoolMgr(){
        _SweepDirtyPools();
        ////All dirty pools should be recycled by now.
        assert(_dirtyPools.empty());
        ////Retire current pool
        //_currentPool = nullptr;
//
        while (!_freePools.empty()) {
            auto pool = _freePools.front();
            _freePools.pop();
            VK_DEV_CALL(_dev, vkDestroyDescriptorPool(_dev->LogicalDev(), pool, nullptr));
        }
    }
    
    void _DescriptorPoolMgr::_SweepDirtyPools() {
        uint32_t end = _dirtyPools.size();
        uint32_t i = 0;
        while(i < end) {
            Container* pool = _dirtyPools[i].get();
            auto refCnt = pool->refCnt.load(std::memory_order_acquire);
            if(refCnt != 0) {
                ++i;
            } else {
                --end;
                std::swap(_dirtyPools[i], _dirtyPools[end]);
            }
        }

        for(i = end; i < _dirtyPools.size(); ++i) {
            Container* container = _dirtyPools[i].get();
            if(container->isDedicatedAlloc) {
                VK_DEV_CALL(_dev,
                    vkDestroyDescriptorPool(_dev->LogicalDev(),  container->pool, 0));
            }
            else {
                VK_CHECK(VK_DEV_CALL(_dev,
                    vkResetDescriptorPool(_dev->LogicalDev(), container->pool, 0)));
                _freePools.push(container->pool);
            }
        }

        _dirtyPools.erase(_dirtyPools.begin() + end, _dirtyPools.end());
    }

    VkDescriptorPool _DescriptorPoolMgr::_GetOnePool(){
        //Should only be called from allocate
        //std::scoped_lock l{_m_pool};

        if(_freePools.empty()) {
            _SweepDirtyPools();
        }

        VkDescriptorPool rawPool;
        if(!_freePools.empty()){
            rawPool = _freePools.front();
            _freePools.pop();
        } else {
            rawPool = _CreatePool(_maxSetsPerPool, _maxDescriptorsPerPool);
        }

        return rawPool;
    }


    VkDescriptorPool _DescriptorPoolMgr::_CreatePool(
        uint32_t maxSetsPerPool,
        uint32_t maxDescriptorsPerPool
    ) {
        VkDescriptorPoolSize poolSize {};
        poolSize.type = _poolType;
        poolSize.descriptorCount = maxDescriptorsPerPool;
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags =
            _dev->GetAdapter().GetAdapterInfo().resourceBindingModel
                != ResourceBindingModel::FixedBindings
                    ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
                    : 0;
        // VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_EXT can be added in
        // the future if we decide to have CPU only descriptor heaps.
        pool_info.maxSets = maxSetsPerPool;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &poolSize;

        // Need special treatment for mutable type pools
        static constexpr VkMutableDescriptorTypeListEXT mutableTypeList {
            .descriptorTypeCount = sizeof(kMutableDescTypes) / sizeof(kMutableDescTypes[0]),
            .pDescriptorTypes = kMutableDescTypes
        };

        static constexpr VkMutableDescriptorTypeCreateInfoEXT mutablePoolInfo {
            .sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
            .mutableDescriptorTypeListCount = 1,
            .pMutableDescriptorTypeLists = &mutableTypeList,
        };

        if(_poolType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT) {
            pool_info.pNext = &mutablePoolInfo;
        }

        VkDescriptorPool descriptorPool;
        VK_CHECK(VK_DEV_CALL(_dev,
            vkCreateDescriptorPool(_dev->LogicalDev(), &pool_info, nullptr, &descriptorPool)));

        return descriptorPool;
    }

    
    _DescriptorSet _DescriptorPoolMgr::Allocate(
        VkDescriptorSetLayout layout,
        uint32_t descriptorCnt, // Total descriptor counts. We can't get this info from
                            //   VkDescriptorSetLayout.
        bool isVariableCnt // Enables variable count
    ) {
        _DescriptorSet allocated {};

        VkDescriptorSetAllocateInfo allocInfo;
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo {};
        uint32_t variableCount = 0;

        if(isVariableCnt) {
            variableCount = descriptorCnt;
            variableCountInfo.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variableCountInfo.descriptorSetCount = 1;
            variableCountInfo.pDescriptorCounts = &variableCount;
            allocInfo.pNext = &variableCountInfo;
        }

        {
            //Critical section guard
            std::scoped_lock l{_m_pool};

            // Make a dedicated allocation if descriptorCnt is larger than current
            // block size
            if(descriptorCnt >= _maxDescriptorsPerPool) {
                auto dedicatedPool = _CreatePool(1, descriptorCnt);
                allocInfo.descriptorPool = dedicatedPool;
                // Should not fail the dedicated allocation
                VkDescriptorSet set;
                VK_CHECK(VK_DEV_CALL(_dev,
                    vkAllocateDescriptorSets(_dev->LogicalDev(), &allocInfo, &set)));

                auto pContainer = std::make_unique<Container>(
                    /*.pool */ dedicatedPool,
                    /*.refCnt */ 0, // _DescriptorSet ctor will increment ref cnt
                    /*.isDedicatedAlloc */ true
                );

                allocated = _DescriptorSet(
                    pContainer.get(), set
                );

                _dirtyPools.push_back(std::move(pContainer));
            } 
            else {
                bool fromFreshPool = false;
                if(!_currentPool) {
                    fromFreshPool = true;
                    auto newPool = _GetOnePool();
                    auto pContainer = std::make_unique<Container>(
                        newPool
                    );
                    _currentPool = pContainer.get();
                    _dirtyPools.push_back(std::move(pContainer));
                }

                //Try to allocate from current pool.
                auto vkPool = _currentPool->pool;
                allocInfo.descriptorPool = vkPool;

                VkDescriptorSet set;
                VkResult result = VK_DEV_CALL(_dev,
                    vkAllocateDescriptorSets(_dev->LogicalDev(), &allocInfo, &set));

                switch (result) {
                    case VK_SUCCESS: break;

                    case VK_ERROR_FRAGMENTED_POOL:
                    case VK_ERROR_OUT_OF_POOL_MEMORY:

                    if(!fromFreshPool){
                        //Fetch a new pool
                        auto newPool = _GetOnePool();

                        //Try to allocate from a clean pool
                        allocInfo.descriptorPool = newPool;
                        result = VK_DEV_CALL(_dev,
                            vkAllocateDescriptorSets(_dev->LogicalDev(), &allocInfo, &set));
                        if(result != VK_SUCCESS){
                            //Still can't allocate, maybe the set is too large?
                            //Clean up
                            _freePools.push(newPool);
                            return allocated;
                        }

                        //Allocate succeeded, seems like the current pool is full
                        //Add to dirty pools

                        //Wrap the raw pool
                        auto pContainer = std::make_unique<Container>(
                            newPool
                        );

                        //change current pool to new pool
                        _currentPool = pContainer.get();
                        _dirtyPools.push_back(std::move(pContainer));
                        break;
                    } 

                    default:
                        return allocated;
                }

                allocated = _DescriptorSet(
                    _currentPool, set
                );
            }
        }
        //Release the mutex to allow old container to do its clean-ups
        
        return allocated;
    }


}
