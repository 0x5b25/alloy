#pragma once

#include <volk.h>

#include "alloy/DescriptorHeap.hpp"

#include "VkDescriptorPoolMgr.hpp"
#include "VulkanBindableResource.hpp"

#include <cstdint>
#include <vector>

namespace alloy::vk
{
    class VulkanDevice;

    // T2 resource descriptor heap, backed by a single VK_DESCRIPTOR_TYPE_MUTABLE_EXT
    // descriptor set sized to capacity, allocated from the device's shared mutable pool
    // against the device's universal resource-heap DSL. Indexed Write/Clear address the
    // set's binding 0 by array element. The heap owns strong references to written
    // resources until they are cleared/overwritten.
    class VulkanResourceDescriptorHeap final : public IResourceDescriptorHeap {
        common::sp<VulkanDevice> _dev;
        Description _desc;

        _DescriptorSet _heapSet;

        // Type-erased lifetime anchors, sized to capacity. Null = unwritten.
        std::vector<common::sp<IBindableResource>> _entries;

        VulkanResourceDescriptorHeap(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

    public:
        ~VulkanResourceDescriptorHeap() override;

        static common::sp<IResourceDescriptorHeap> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

        const Description& GetDesc() const override { return _desc; }

        void Write(
            ResourceDescriptorIndex index,
            const ResourceDescriptorWrite& write) override;
        void WriteRange(
            ResourceDescriptorIndex firstIndex,
            std::span<const ResourceDescriptorWrite> writes) override;
        void Clear(ResourceDescriptorIndex index) override;
        void ClearRange(ResourceDescriptorIndex firstIndex, std::uint32_t count) override;

        // Internal: shared write primitive used by both the public indexed Write and by
        // descriptor ranges. `resource` may be null to clear the entry.
        void WriteSlot(
            std::uint32_t absoluteIndex,
            const ResourceDescriptorWrite& write,
            const common::sp<IBindableResource>& lifetimeRef);
        void ClearSlot(std::uint32_t absoluteIndex);

        VkDescriptorSet GetHeapSet() const { return _heapSet.GetHandle(); }
    };

    // T2 sampler descriptor heap. Single SAMPLER descriptor set sized to capacity.
    class VulkanSamplerDescriptorHeap final : public ISamplerDescriptorHeap {
        common::sp<VulkanDevice> _dev;
        Description _desc;

        _DescriptorSet _heapSet;

        std::vector<common::sp<ISampler>> _entries; // null = unwritten

        VulkanSamplerDescriptorHeap(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

    public:
        ~VulkanSamplerDescriptorHeap() override;

        static common::sp<ISamplerDescriptorHeap> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

        const Description& GetDesc() const override { return _desc; }

        void Write(
            SamplerDescriptorIndex index,
            const SamplerDescriptorWrite& sampler) override;
        void WriteRange(
            SamplerDescriptorIndex firstIndex,
            std::span<const SamplerDescriptorWrite> samplers) override;
        void Clear(SamplerDescriptorIndex index) override;
        void ClearRange(SamplerDescriptorIndex firstIndex, std::uint32_t count) override;

        // Internal: shared write primitive. `sampler` may be null to clear.
        void WriteSlot(std::uint32_t absoluteIndex, const common::sp<ISampler>& sampler);

        VkDescriptorSet GetHeapSet() const { return _heapSet.GetHandle(); }

    };
}
