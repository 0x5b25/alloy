#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "alloy/DescriptorHeap.hpp"

#include <vector>

namespace alloy::vk
{
    class VulkanDevice;

    class VulkanResourceDescriptorHeap : public IResourceDescriptorHeap {
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

        void* GetNativeHandle() const override { return (void*)_buffer; }

        VkBuffer GetBuffer() const { return _buffer; }
        VkDeviceAddress GetDeviceAddress() const;
        VkDeviceSize GetSlotStride() const { return _slotStride; }
        VkDeviceSize GetSizeInBytes() const { return _sizeInBytes; }

    private:
        VulkanResourceDescriptorHeap(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

        bool Init();
        bool ValidateRange(std::uint32_t firstIndex, std::uint32_t count) const;
        void* GetSlotPtr(std::uint32_t index) const;
        void FlushSlot(std::uint32_t index, VkDeviceSize size) const;

        void WriteDescriptor(std::uint32_t index, const UniformBufferDescriptor& write);
        void WriteDescriptor(std::uint32_t index, const ReadOnlyStorageBufferDescriptor& write);
        void WriteDescriptor(std::uint32_t index, const ReadWriteStorageBufferDescriptor& write);
        void WriteDescriptor(std::uint32_t index, const SampledTextureDescriptor& write);
        void WriteDescriptor(std::uint32_t index, const StorageTextureDescriptor& write);

        common::sp<VulkanDevice> _dev;
        Description _desc;
        VkBuffer _buffer = VK_NULL_HANDLE;
        VmaAllocation _allocation = VK_NULL_HANDLE;
        void* _mapped = nullptr;
        VkDeviceSize _slotStride = 0;
        VkDeviceSize _sizeInBytes = 0;
        std::vector<ResourceDescriptorWrite> _writes;
    };

    class VulkanSamplerDescriptorHeap : public ISamplerDescriptorHeap {
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

        void* GetNativeHandle() const override { return (void*)_buffer; }

        VkBuffer GetBuffer() const { return _buffer; }
        VkDeviceAddress GetDeviceAddress() const;
        VkDeviceSize GetSlotStride() const { return _slotStride; }
        VkDeviceSize GetSizeInBytes() const { return _sizeInBytes; }

    private:
        VulkanSamplerDescriptorHeap(
            const common::sp<VulkanDevice>& dev,
            const Description& desc);

        bool Init();
        bool ValidateRange(std::uint32_t firstIndex, std::uint32_t count) const;
        void* GetSlotPtr(std::uint32_t index) const;
        void FlushSlot(std::uint32_t index, VkDeviceSize size) const;

        common::sp<VulkanDevice> _dev;
        Description _desc;
        VkBuffer _buffer = VK_NULL_HANDLE;
        VmaAllocation _allocation = VK_NULL_HANDLE;
        void* _mapped = nullptr;
        VkDeviceSize _slotStride = 0;
        VkDeviceSize _sizeInBytes = 0;
        std::vector<SamplerDescriptorWrite> _writes;
    };
} // namespace alloy::vk
