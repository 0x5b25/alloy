#pragma once

#include <volk.h>

#include "alloy/common/RefCnt.hpp"
#include "alloy/common/Macros.h"

#include <cstdint>
#include <queue>
#include <unordered_set>
#include <mutex>

// Vulkan multithreading safety:
// From: https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html
// ...
// A descriptor pool maintains a pool of descriptors, from which descriptor sets are allocated.
// Descriptor pools are externally synchronized, meaning that the application must not allocate
// and/or free descriptor sets from the same pool in multiple threads simultaneously.
// ...
// VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT specifies that if descriptors in this binding 
// are updated between when the descriptor set is bound in a command buffer and when that 
// command buffer is submitted to a queue, then the submission will use the most recently set 
// descriptors for this binding and the updates do not invalidate the command buffer.
// Descriptor bindings created with this flag are also partially exempt from the external 
// synchronization requirement in vkUpdateDescriptorSetWithTemplateKHR and vkUpdateDescriptorSets.
// Multiple descriptors with this flag set can be updated concurrently in different threads,
// though the same descriptor must not be updated concurrently by two threads. Descriptors with
// this flag set can be updated concurrently with the set being bound to a command buffer in
// another thread, but not concurrently with the set being reset or freed.
// ...

namespace alloy::vk {

    class _DescriptorSet;
    class VulkanDevice;
    class VulkanResourceLayout;

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
         struct Container {
            VkDescriptorPool pool;
            std::atomic<uint32_t> refCnt;
         };

    private:
        VulkanDevice* _dev;

        VkDescriptorType _poolType;
        unsigned _maxSetsPerPool;
        unsigned _maxDescriptorsPerPool;

        //'Clean' pools.
        std::queue<VkDescriptorPool> _freePools;
        //previously full pools, some sets might be freed, but at least one set is in use.
        std::vector<std::unique_ptr<Container>> _dirtyPools;
        //Currently active pool, that is not full.
        Container* _currentPool;

        std::mutex _m_pool;

        // Walk each "dirty pools" and gather no outstanding ref ones
        // into _freePools. Not thread safe.
        void _SweepDirtyPools();

        VkDescriptorPool _GetOnePool();
        VkDescriptorPool _CreatePool(
            uint32_t maxSetsPerPool,
            uint32_t maxDescriptorsPerPool);

    public:
        struct CreateArgs {
            VulkanDevice* dev;
            VkDescriptorType type;
            unsigned maxSetsPerPool;
            unsigned maxDescriptorsPerPool;
        };

        
        //Must call Init
        _DescriptorPoolMgr(const CreateArgs& args);
        ~_DescriptorPoolMgr();

        _DescriptorSet Allocate(
            
            VkDescriptorSetLayout layout,
            bool descriptorCnt, // Total descriptor counts. We can't get this info from
                                //   VkDescriptorSetLayout.
            bool isVariableCnt // Enables variable count
        );
    };



    class _DescriptorSet{
        DISABLE_COPY_AND_ASSIGN(_DescriptorSet);

        _DescriptorPoolMgr::Container* _pool;
        VkDescriptorSet _descSet;

    public:
        _DescriptorSet()
            : _pool{nullptr}
            , _descSet{VK_NULL_HANDLE}
        {}

        _DescriptorSet(
            _DescriptorPoolMgr::Container* pool,
            VkDescriptorSet set    
        )
            : _pool(pool)
            , _descSet(set)
        {}


        ~_DescriptorSet(){
            //No need to vkDestroy the desc set
            //since the whole will be reseted once
            //there is no reference to the pool.
            if(_pool)
                _pool->refCnt.fetch_sub(1, std::memory_order_release);
        }

        //move semantics support
        _DescriptorSet(_DescriptorSet&& another)
            : _pool(another._pool)
            , _descSet(another._descSet)
        {
            another._pool = nullptr;
            another._descSet = VK_NULL_HANDLE;
        }
        
        _DescriptorSet& operator=(_DescriptorSet&& that){
            _pool = that._pool;
            _descSet = that._descSet;

            that._pool = nullptr;
            that._descSet = VK_NULL_HANDLE;
            return *this;
        }

        const VkDescriptorSet& GetHandle() const {return _descSet;}

    };
}