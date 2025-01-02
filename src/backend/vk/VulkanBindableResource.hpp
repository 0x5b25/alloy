#pragma once

#include <volk.h>

#include "veldrid/BindableResource.hpp"

#include <unordered_set>
#include <functional>
#include <vector>

#include "VkDescriptorPoolMgr.hpp"

namespace Veldrid{

    class VulkanDevice;
    class VulkanTexture;

    class VulkanResourceLayout : public ResourceLayout{

    public:
        struct BindSetInfo{
            uint32_t regSpaceDesignated; //Value described in the description
            uint32_t setIndexAllocated; //Actually allocated value
            std::vector<uint32_t> elementIdInList;
            VkDescriptorSetLayout layout;
            VK::priv::DescriptorResourceCounts resourceCounts;
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
        
        std::vector<ResourceBindInfo> _bindings;
        //std::uint32_t _dynamicBufferCount;

        VulkanResourceLayout(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) : ResourceLayout(dev, desc){}

    public:
        ~VulkanResourceLayout();

        static sp<ResourceLayout> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
        );

        //const VkDescriptorSetLayout& GetHandle() const {return _dsl;}
        //std::uint32_t GetDynamicBufferCount() const {return _dynamicBufferCount;}
        //const Veldrid::VK::priv::DescriptorResourceCounts& GetResourceCounts() const {return _drcs;}

        const std::vector<ResourceBindInfo>& GetBindings() const {return _bindings;}
    };

    class VulkanResourceSet : public ResourceSet{
    public:
        using ElementVisitor = std::function<void(VulkanResourceLayout*)>;
    private:

        std::vector<Veldrid::VK::priv::_DescriptorSet> _descSet;

        std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;

        VulkanResourceSet(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) 
            : ResourceSet(dev, desc)
        {}

    public:
        ~VulkanResourceSet();

        static sp<ResourceSet> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
        );

        const auto& GetHandle() const { return _descSet; }


        void TransitionImageLayoutsIfNeeded(VkCommandBuffer cb);
        void VisitElements(ElementVisitor visitor);
    };
}