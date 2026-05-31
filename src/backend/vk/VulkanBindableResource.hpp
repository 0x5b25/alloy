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

    // Use flattened descriptor sets. Each ResourceKind is
    // a descriptor set. descriptors are assigned using 
    // API order from IResourceLayout::Descripton:shaderResources
    class VulkanResourceLayout : public IResourceLayout{

    public:
        struct BindingInfo{
            // Save all designation info for later use in shader 
            // remapper
            //the register index in hlsl, map to bindingSlot
            uint32_t regIdxDesignated; 
            //the register index allocated
            uint32_t bindSlotAllocated; 
            //the register space in hlsl, map to bindingSpace
            uint32_t regSpaceDesignated;
            //the register space allocated
            uint32_t bindSetAllocated;
            
            // Back mapping into IResourceLayout::Descripton:shaderResources
            // for later lookup
            uint32_t indexInShaderResources;
        };

        struct ResourceSetInfo {
            //IBindableResource::ResourceKind kind;
            VkDescriptorType type;
            VkDescriptorSetLayout layout;
            uint32_t elementCnt;
            // Back mapping into IResourceLayout::Descripton:shaderResources
            // for later lookup
            std::vector<BindingInfo> bindings;
        };

        struct PushConstantInfo {
            uint32_t bindingSlot;
            uint32_t bindingSpace;
            uint32_t sizeInDwords;
            uint32_t offsetInDwords;
        };

        struct SlotLocation {
            uint32_t setIndexAllocated; // Match with ResourceSetInfo array
            uint32_t bindingAllocated; // The descriptor index within ResourceSet
            uint32_t linearResourceOffset; // which one in the resource set boundResources
        };

        // Fixed T2 (DescriptorHeap) pipeline-layout set ABI. See road_to_bindless.md.
        enum T2SetIndex : uint32_t {
            T2Set_ResourceHeap     = 0, // device universal mutable heap DSL (non-owning)
            T2Set_SamplerHeap      = 1, // device universal sampler heap DSL (non-owning)
            T2Set_Count
        };

    private:
        common::sp<VulkanDevice> _dev;

        std::vector<ResourceSetInfo> _sets;
        std::vector<SlotLocation> _slotLocations;
        //std::uint32_t _dynamicBufferCount;

        std::vector<PushConstantInfo> _pushConstants;
        uint32_t _pushConstantSize;

        Description _desc;

        VulkanResourceLayout(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        ) 
            : IResourceLayout({})
            , _dev(dev)
            , _desc(desc)
        { }

        
        static common::sp<IResourceLayout> _MakeFixedSize(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

        
        static common::sp<IResourceLayout> _MakeT2Bindless(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

    public:
        ~VulkanResourceLayout();

        static common::sp<IResourceLayout> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

        const Description& GetDesc() const override { return _desc; }

        //const VkDescriptorSetLayout& GetHandle() const {return _dsl;}
        //std::uint32_t GetDynamicBufferCount() const {return _dynamicBufferCount;}
        //const alloy::VK::priv::DescriptorResourceCounts& GetResourceCounts() const {return _drcs;}

        const std::vector<ResourceSetInfo>& GetResSetInfo() const {return _sets;}
        const SlotLocation& GetSlotLocation(uint32_t layoutSlot) const {return _slotLocations.at(layoutSlot);}
        const std::vector<PushConstantInfo>& GetPushConstants() const {return _pushConstants;}
        std::uint32_t GetPushConstantSize() const {return _pushConstantSize;}
    };

    class VulkanResourceSetBase {
    public:
        using ElementVisitor = std::function<void(VulkanResourceLayout*)>;

    protected:
        
        common::sp<VulkanDevice> _dev;
        common::sp<VulkanResourceLayout> _layout;

        std::vector<alloy::vk::_DescriptorSet> _descSet;
        std::vector<common::sp<IBindableResource>> _boundResources;

        std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;

        bool _isMutable;

        void AllocateDescriptorSets();
        void UpdateInternal(const std::span<const IMutableResourceSet::WriteBinding>& writes);

        VulkanResourceSetBase(
            const common::sp<VulkanDevice>& dev,
            const common::sp<VulkanResourceLayout>& layout,
            bool isMutable
        );

    public:
        const VulkanResourceLayout& GetLayout() const { return *_layout; }
        IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        );
        const std::vector<common::sp<IBindableResource>>& GetBoundResources() const { return _boundResources; }
        const auto& GetHandle() const { return _descSet; }
    };

    class VulkanResourceSet : public IResourceSet, public VulkanResourceSetBase{
        VulkanResourceSet(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

    public:
        ~VulkanResourceSet() override;

        virtual const IResourceLayout& GetLayout() const override {
            return VulkanResourceSetBase::GetLayout();
        }

        
        virtual IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        ) override {
            return VulkanResourceSetBase::GetBoundResource(
                layoutSlot,
                firstArrayElement
            );
        }

        static common::sp<IResourceSet> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );
    };

    class VulkanMutableResourceSet : public IMutableResourceSet, public VulkanResourceSetBase {
        VulkanMutableResourceSet(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

    public:
        ~VulkanMutableResourceSet() override;

        static common::sp<IMutableResourceSet> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

        virtual const IResourceLayout& GetLayout() const override {
            return VulkanResourceSetBase::GetLayout();
        }

        
        virtual IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        ) override {
            return VulkanResourceSetBase::GetBoundResource(
                layoutSlot,
                firstArrayElement
            );
        }

        void Update(const std::span<const WriteBinding>& writes) override;
    };
}
