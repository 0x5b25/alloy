#pragma once

#include <volk.h>

#include "alloy/BindableResource.hpp"

#include <unordered_set>
#include <functional>
#include <vector>

#include "VkDescriptorPoolMgr.hpp"

namespace alloy::vk{

    class VulkanDevice;
    class VulkanTexture;

    class VulkanResourceLayout : public IResourceLayout{

    public:
        struct BindSetInfo{
            uint32_t regSpaceDesignated; //Value described in the description
            uint32_t setIndexAllocated; //Actually allocated value
            std::vector<uint32_t> elementIdInList;
            VkDescriptorSetLayout layout;
            alloy::vk::DescriptorResourceCounts resourceCounts;
            uint32_t dynamicBufferCount;
        };

        enum class BindType {
            Sampler, //Maps to DX12 sampler type
            UniformBuffer, //Maps to DX12 constant buffer type
            ShaderResourceReadOnly,//Maps to DX12 SRV type
            ShaderResourceReadWrite,//Maps to DX12 UAV type
        };

        struct ResourceBindInfo {
            //IBindableResource::ResourceKind kind;
            BindType type;
            uint32_t baseSetIndex;
            std::vector<BindSetInfo> sets;
        };

    private:
        common::sp<VulkanDevice> _dev;
        
        std::vector<ResourceBindInfo> _bindings;
        //std::uint32_t _dynamicBufferCount;

        VulkanResourceLayout(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        ) 
            : IResourceLayout(desc)
            , _dev(dev)
        { }

    public:
        ~VulkanResourceLayout();

        static common::sp<IResourceLayout> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

        //const VkDescriptorSetLayout& GetHandle() const {return _dsl;}
        //std::uint32_t GetDynamicBufferCount() const {return _dynamicBufferCount;}
        //const alloy::VK::priv::DescriptorResourceCounts& GetResourceCounts() const {return _drcs;}

        const std::vector<ResourceBindInfo>& GetBindings() const {return _bindings;}
    };

    class VulkanResourceSet : public IResourceSet{
    public:
        using ElementVisitor = std::function<void(VulkanResourceLayout*)>;
    private:
        
        common::sp<VulkanDevice> _dev;

        std::vector<alloy::vk::_DescriptorSet> _descSet;

        std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;

        VulkanResourceSet(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        ) 
            : IResourceSet(desc)
            , _dev(dev)
        {}

    public:
        ~VulkanResourceSet();

        static common::sp<IResourceSet> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

        const auto& GetHandle() const { return _descSet; }


        //void TransitionImageLayoutsIfNeeded(VkCommandBuffer cb);
        //void VisitElements(ElementVisitor visitor);
    };
}