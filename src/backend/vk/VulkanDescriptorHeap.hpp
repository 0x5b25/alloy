#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "alloy/DescriptorHeap.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace alloy::vk
{
    class VulkanDevice;

    class VulkanResourceDescriptorHeap : public IResourceDescriptorHeap {
        common::sp<VulkanDevice> _dev;
        Description _desc;

        VkBuffer _buffer = VK_NULL_HANDLE;
        VmaAllocation _allocation = VK_NULL_HANDLE;
        void* _mappedData = nullptr;
        VkDeviceAddress _deviceAddress = 0;
        VkDeviceSize _stride = 0;
        VkDeviceSize _bufferSize = 0;

        std::vector<std::optional<ResourceDescriptorWrite>> _entries;

        VulkanResourceDescriptorHeap(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

        void EncodeDescriptor(
            ResourceDescriptorIndex index,
            const ResourceDescriptorWrite& write);
        void ClearDescriptor(ResourceDescriptorIndex index);

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

        VkBuffer GetHandle() const { return _buffer; }
        VkDeviceAddress GetDeviceAddress() const { return _deviceAddress; }
        VkDeviceSize GetStride() const { return _stride; }
        VkDeviceSize GetBufferSize() const { return _bufferSize; }
        const VulkanDescriptorHeapSetLayout& GetLayoutABI() const {
            return _layoutABI->ResourceLayout();
        }

        void* GetNativeHandle() const override {
            return reinterpret_cast<void*>(_buffer);
        }
    };

    class VulkanSamplerDescriptorHeap : public ISamplerDescriptorHeap {
        common::sp<VulkanDevice> _dev;
        common::sp<VulkanDescriptorHeapLayoutABI> _layoutABI;
        Description _desc;

        VkBuffer _buffer = VK_NULL_HANDLE;
        VmaAllocation _allocation = VK_NULL_HANDLE;
        void* _mappedData = nullptr;
        VkDeviceAddress _deviceAddress = 0;
        VkDeviceSize _stride = 0;
        VkDeviceSize _bufferSize = 0;

        std::vector<SamplerDescriptorWrite> _entries;

        VulkanSamplerDescriptorHeap(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

        void EncodeDescriptor(
            SamplerDescriptorIndex index,
            const SamplerDescriptorWrite& sampler);
        void ClearDescriptor(SamplerDescriptorIndex index);

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

        VkBuffer GetHandle() const { return _buffer; }
        VkDeviceAddress GetDeviceAddress() const { return _deviceAddress; }
        VkDeviceSize GetStride() const { return _stride; }
        VkDeviceSize GetBufferSize() const { return _bufferSize; }
        const VulkanDescriptorHeapSetLayout& GetLayoutABI() const {
            return _layoutABI->SamplerLayout();
        }

        void* GetNativeHandle() const override {
            return reinterpret_cast<void*>(_buffer);
        }
    };
}
