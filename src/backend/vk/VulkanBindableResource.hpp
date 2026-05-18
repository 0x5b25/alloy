#pragma once

#include <volk.h>

#include "alloy/BindableResource.hpp"

#include <unordered_set>
#include <functional>
#include <span>
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
            // Indices into IResourceLayout::Description::shaderResources
            // owned by this flattened Vulkan descriptor set.
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

        struct PushConstantInfo {
            uint32_t bindingSlot;
            uint32_t bindingSpace;
            uint32_t sizeInDwords;
            uint32_t offsetInDwords;
        };

        struct SlotLocation {
            uint32_t setIndexAllocated;
            uint32_t binding;
            bool valid = false;
        };

    private:
        common::sp<VulkanDevice> _dev;
        
        std::vector<ResourceBindInfo> _bindings;
        std::vector<SlotLocation> _slotLocations;
        //std::uint32_t _dynamicBufferCount;

        std::vector<PushConstantInfo> _pushConstants;
        uint32_t _pushConstantSize;

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
        const SlotLocation& GetSlotLocation(uint32_t layoutSlot) const {return _slotLocations.at(layoutSlot);}
        const std::vector<PushConstantInfo>& GetPushConstants() const {return _pushConstants;}
        std::uint32_t GetPushConstantSize() const {return _pushConstantSize;}
    };

    class VulkanResourceSetBase {
    public:
        using ElementVisitor = std::function<void(VulkanResourceLayout*)>;

    protected:
        
        common::sp<VulkanDevice> _dev;
        common::sp<IResourceLayout> _layout;

        std::vector<alloy::vk::_DescriptorSet> _descSet;
        std::vector<common::sp<IBindableResource>> _boundResources;

        std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;

        void AllocateDescriptorSets();
        void UpdateInternal(const std::span<const IMutableResourceSet::WriteBinding>& writes);

        VulkanResourceSetBase(
            const common::sp<VulkanDevice>& dev,
            const common::sp<IResourceLayout>& layout
        )
            : _dev(dev)
            , _layout(layout)
        {}

    public:
        const common::sp<IResourceLayout>& GetLayout() const { return _layout; }
        const std::vector<common::sp<IBindableResource>>& GetBoundResources() const { return _boundResources; }
        const auto& GetHandle() const { return _descSet; }
    };

    class VulkanResourceSet : public IResourceSet, public VulkanResourceSetBase{
        VulkanResourceSet(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        ) 
            : IResourceSet(desc)
            , VulkanResourceSetBase(dev, desc.layout)
        {}

    public:
        ~VulkanResourceSet();

        static common::sp<IResourceSet> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );
    };

    class VulkanMutableResourceSet : public IMutableResourceSet, public VulkanResourceSetBase {
        VulkanMutableResourceSet(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        )
            : IMutableResourceSet(desc)
            , VulkanResourceSetBase(dev, desc.layout)
        {}

    public:
        ~VulkanMutableResourceSet();

        static common::sp<IMutableResourceSet> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

        void Update(const std::span<const WriteBinding>& writes) override;
    };
}
