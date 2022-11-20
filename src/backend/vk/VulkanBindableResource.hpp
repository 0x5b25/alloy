#pragma once

#include <volk.h>

#include "veldrid/BindableResource.hpp"

#include <unordered_set>

#include "VkDescriptorPoolMgr.hpp"

namespace Veldrid{

    class VulkanDevice;
    class VulkanTexture;

    class VulkanResourceLayout : public ResourceLayout{

        VkDescriptorSetLayout _dsl;

        std::uint32_t _dynamicBufferCount;
        DescriptorResourceCounts _drcs;

        VulkanResourceLayout(
            const sp<VulkanDevice>& dev,
            const Description& desc
        ) : ResourceLayout(dev, desc){}

    public:
        ~VulkanResourceLayout();

        static sp<ResourceLayout> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
        );

        const VkDescriptorSetLayout& GetHandle() const {return _dsl;}
        std::uint32_t GetDynamicBufferCount() const {return _dynamicBufferCount;}
        const DescriptorResourceCounts& GetResourceCounts() const {return _drcs;}
    };

    class VulkanResourceSet : public ResourceSet{
    public:
        using ElementVisitor = std::function<void(VulkanResourceLayout*)>;
    private:

        _DescriptorSet _descSet;

        std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;

        VulkanResourceSet(
            const sp<VulkanDevice>& dev,
            _DescriptorSet&& set,
            const Description& desc
        ) 
            : ResourceSet(dev, desc)
            , _descSet(std::move(set))
        
        {}

    public:
        ~VulkanResourceSet();

        static sp<ResourceSet> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
        );

        const VkDescriptorSet& GetHandle() const { return _descSet.GetHandle(); }


        void TransitionImageLayoutsIfNeeded(VkCommandBuffer cb);
        void VisitElements(ElementVisitor visitor);
    };
}