#include "VulkanDescriptorHeap.hpp"

#include "alloy/common/Common.hpp"

#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace alloy::vk
{
    namespace {

        // Concrete VkDescriptorType for each ResourceDescriptorWrite alternative.
        VkDescriptorType ConcreteType(const ResourceDescriptorWrite& write) {
            return std::visit([](const auto& d) -> VkDescriptorType {
                using D = std::decay_t<decltype(d)>;
                if constexpr(std::is_same_v<D, UniformBufferDescriptor>)
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                else if constexpr(std::is_same_v<D, ReadOnlyStorageBufferDescriptor> ||
                                  std::is_same_v<D, ReadWriteStorageBufferDescriptor>)
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                else if constexpr(std::is_same_v<D, SampledTextureDescriptor>)
                    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                else // StorageTextureDescriptor
                    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            }, write);
        }

        // Extract the underlying bindable resource from a write for lifetime tracking.
        common::sp<IBindableResource> LifetimeRefOf(const ResourceDescriptorWrite& write) {
            return std::visit([](const auto& d) -> common::sp<IBindableResource> {
                using D = std::decay_t<decltype(d)>;
                if constexpr(std::is_same_v<D, UniformBufferDescriptor> ||
                             std::is_same_v<D, ReadOnlyStorageBufferDescriptor> ||
                             std::is_same_v<D, ReadWriteStorageBufferDescriptor>)
                    return d.buffer;
                else
                    return d.texture;
            }, write);
        }
    }

    //=====================================================================
    // VulkanResourceDescriptorHeap
    //=====================================================================

    VulkanResourceDescriptorHeap::VulkanResourceDescriptorHeap(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
        , _entries(desc.capacity)
    { }

    VulkanResourceDescriptorHeap::~VulkanResourceDescriptorHeap() = default;

    common::sp<IResourceDescriptorHeap> VulkanResourceDescriptorHeap::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
    {

        auto* heap = new VulkanResourceDescriptorHeap(dev, desc);
        heap->_heapSet = dev->AllocateDescriptorSet(
            dev->GetT2ResourceHeapDSL(),
            VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
            desc.capacity,
            /*isVariableCnt*/ true,
            /*isMutableSet*/ true);
        return common::sp<IResourceDescriptorHeap>(heap);
    }

    void VulkanResourceDescriptorHeap::WriteSlot(
        std::uint32_t absoluteIndex,
        const ResourceDescriptorWrite& write,
        const common::sp<IBindableResource>& lifetimeRef)
    {
        VkWriteDescriptorSet vkWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        vkWrite.dstSet = _heapSet.GetHandle();
        vkWrite.dstBinding = 0;
        vkWrite.dstArrayElement = absoluteIndex;
        vkWrite.descriptorCount = 1;
        vkWrite.descriptorType = ConcreteType(write);

        VkDescriptorBufferInfo bufferInfo{};
        VkDescriptorImageInfo imageInfo{};

        std::visit([&](const auto& d) {
            using D = std::decay_t<decltype(d)>;
            if constexpr(std::is_same_v<D, UniformBufferDescriptor> ||
                         std::is_same_v<D, ReadOnlyStorageBufferDescriptor> ||
                         std::is_same_v<D, ReadWriteStorageBufferDescriptor>) {
                assert(d.buffer && "Descriptor heap write has a null buffer range.");
                const auto* vkBuffer = common::PtrCast<VulkanBuffer>(d.buffer->GetBufferObject().get());
                bufferInfo.buffer = vkBuffer->GetHandle();
                bufferInfo.offset = d.buffer->GetShape().GetOffsetInBytes();
                bufferInfo.range = d.buffer->GetShape().GetSizeInBytes();
                vkWrite.pBufferInfo = &bufferInfo;
            } else { // texture
                assert(d.texture && "Descriptor heap write has a null texture view.");
                const auto* vkView = common::PtrCast<VulkanTextureView>(d.texture.get());
                imageInfo.imageView = vkView->GetHandle();
                imageInfo.imageLayout = std::is_same_v<D, StorageTextureDescriptor>
                    ? VK_IMAGE_LAYOUT_GENERAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                vkWrite.pImageInfo = &imageInfo;
            }
        }, write);

        VK_DEV_CALL(_dev,
            vkUpdateDescriptorSets(_dev->LogicalDev(), 1, &vkWrite, 0, nullptr));

        _entries[absoluteIndex] = lifetimeRef;
    }

    void VulkanResourceDescriptorHeap::ClearSlot(std::uint32_t absoluteIndex) {
        // PARTIALLY_BOUND: dropping the strong ref is sufficient; the shader must not read
        // a cleared entry.
        _entries[absoluteIndex].reset();
    }

    void VulkanResourceDescriptorHeap::Write(
        ResourceDescriptorIndex index,
        const ResourceDescriptorWrite& write)
    {
        WriteSlot(index.value, write, LifetimeRefOf(write));
    }

    void VulkanResourceDescriptorHeap::WriteRange(
        ResourceDescriptorIndex firstIndex,
        std::span<const ResourceDescriptorWrite> writes)
    {
        for(std::uint32_t i = 0; i < writes.size(); ++i) {
            WriteSlot(firstIndex.value + i, writes[i], LifetimeRefOf(writes[i]));
        }
    }

    void VulkanResourceDescriptorHeap::Clear(ResourceDescriptorIndex index) {
        ClearSlot(index.value);
    }

    void VulkanResourceDescriptorHeap::ClearRange(
        ResourceDescriptorIndex firstIndex, std::uint32_t count)
    {
        for(std::uint32_t i = 0; i < count; ++i) {
            ClearSlot(firstIndex.value + i);
        }
    }

    //=====================================================================
    // VulkanSamplerDescriptorHeap
    //=====================================================================

    VulkanSamplerDescriptorHeap::VulkanSamplerDescriptorHeap(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
        , _entries(desc.capacity)
    { }

    VulkanSamplerDescriptorHeap::~VulkanSamplerDescriptorHeap() = default;

    common::sp<ISamplerDescriptorHeap> VulkanSamplerDescriptorHeap::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
    {

        auto* heap = new VulkanSamplerDescriptorHeap(dev, desc);
        heap->_heapSet = dev->AllocateDescriptorSet(
            dev->GetT2SamplerHeapDSL(),
            VK_DESCRIPTOR_TYPE_SAMPLER,
            desc.capacity,
            /*isVariableCnt*/ true,
            /*isMutableSet*/ true);
        return common::sp<ISamplerDescriptorHeap>(heap);
    }

    void VulkanSamplerDescriptorHeap::WriteSlot(
        std::uint32_t absoluteIndex, const common::sp<ISampler>& sampler)
    {
        if(!sampler) {
            _entries[absoluteIndex].reset();
            return;
        }

        auto* vkSampler = PtrCast<VulkanSampler>(sampler.get());

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = vkSampler->GetHandle();

        VkWriteDescriptorSet vkWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        vkWrite.dstSet = _heapSet.GetHandle();
        vkWrite.dstBinding = 0;
        vkWrite.dstArrayElement = absoluteIndex;
        vkWrite.descriptorCount = 1;
        vkWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        vkWrite.pImageInfo = &imageInfo;

        VK_DEV_CALL(_dev,
            vkUpdateDescriptorSets(_dev->LogicalDev(), 1, &vkWrite, 0, nullptr));

        _entries[absoluteIndex] = sampler;
    }

    void VulkanSamplerDescriptorHeap::Write(
        SamplerDescriptorIndex index, const SamplerDescriptorWrite& sampler)
    {
        WriteSlot(index.value, sampler);
    }

    void VulkanSamplerDescriptorHeap::WriteRange(
        SamplerDescriptorIndex firstIndex,
        std::span<const SamplerDescriptorWrite> samplers)
    {
        for(std::uint32_t i = 0; i < samplers.size(); ++i) {
            WriteSlot(firstIndex.value + i, samplers[i]);
        }
    }

    void VulkanSamplerDescriptorHeap::Clear(SamplerDescriptorIndex index) {
        _entries[index.value].reset();
    }

    void VulkanSamplerDescriptorHeap::ClearRange(
        SamplerDescriptorIndex firstIndex, std::uint32_t count)
    {
        for(std::uint32_t i = 0; i < count; ++i) {
            _entries[firstIndex.value + i].reset();
        }
    }
}
