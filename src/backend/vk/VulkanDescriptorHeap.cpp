#include "VulkanDescriptorHeap.hpp"

#include "alloy/common/Common.hpp"

#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <variant>

namespace alloy::vk
{
    namespace
    {
        VkDeviceSize AlignUp(VkDeviceSize value, VkDeviceSize alignment)
        {
            if (alignment <= 1) {
                return value;
            }

            return ((value + alignment - 1) / alignment) * alignment;
        }

        VkDeviceSize GetResourceDescriptorStride(
            const VkPhysicalDeviceDescriptorBufferPropertiesEXT& props)
        {
            VkDeviceSize stride = props.uniformBufferDescriptorSize;
            stride = std::max<VkDeviceSize>(stride, props.storageBufferDescriptorSize);
            stride = std::max<VkDeviceSize>(stride, props.sampledImageDescriptorSize);
            stride = std::max<VkDeviceSize>(stride, props.storageImageDescriptorSize);
            return AlignUp(stride, props.descriptorBufferOffsetAlignment);
        }

        VkDescriptorAddressInfoEXT GetBufferAddressInfo(
            VulkanDevice* dev,
            const common::sp<BufferRange>& buffer)
        {
            auto* vkBuffer = common::PtrCast<VulkanBuffer>(buffer->GetBufferObject());
            const auto& shape = buffer->GetShape();

            VkBufferDeviceAddressInfo addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = vkBuffer->GetHandle();

            VkDescriptorAddressInfoEXT descriptorAddress{};
            descriptorAddress.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
            descriptorAddress.address =
                dev->GetFnTable().vkGetBufferDeviceAddress(dev->LogicalDev(), &addressInfo) +
                shape.GetOffsetInBytes();
            descriptorAddress.range = shape.GetSizeInBytes();
            descriptorAddress.format = VK_FORMAT_UNDEFINED;

            return descriptorAddress;
        }

        VkDescriptorImageInfo GetImageInfo(
            const common::sp<ITextureView>& texture,
            VkImageLayout layout)
        {
            auto* vkTextureView = common::PtrCast<VulkanTextureView>(texture.get());

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageView = vkTextureView->GetHandle();
            imageInfo.imageLayout = layout;
            return imageInfo;
        }
    }

    VulkanResourceDescriptorHeap::VulkanResourceDescriptorHeap(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
    { }

    VulkanResourceDescriptorHeap::~VulkanResourceDescriptorHeap()
    {
        if (_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(_dev->Allocator(), _buffer, _allocation);
        }
    }

    common::sp<IResourceDescriptorHeap> VulkanResourceDescriptorHeap::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
    {
        if (dev->GetDevCaps().resourceBindingModel !=
            VulkanDevCaps::ResourceBindingModel::DescriptorBuffer) {
            return nullptr;
        }

        auto* heap = new VulkanResourceDescriptorHeap(dev, desc);
        if (!heap->Init()) {
            delete heap;
            return nullptr;
        }

        return common::sp<IResourceDescriptorHeap>(heap);
    }

    bool VulkanResourceDescriptorHeap::Init()
    {
        if (_desc.capacity == 0) {
            assert(false && "Descriptor heap capacity must be greater than zero.");
            return false;
        }

        const auto& props = _dev->GetDevCaps().descriptorBufferProperties;
        _slotStride = GetResourceDescriptorStride(props);
        _sizeInBytes = _slotStride * _desc.capacity;

        if (_sizeInBytes > props.maxResourceDescriptorBufferRange) {
            assert(false && "Resource descriptor heap exceeds Vulkan descriptor buffer range.");
            return false;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = _sizeInBytes;
        bufferInfo.usage =
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        allocInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo{};
        VkResult result = vmaCreateBuffer(
            _dev->Allocator(),
            &bufferInfo,
            &allocInfo,
            &_buffer,
            &_allocation,
            &allocationInfo);
        if (result != VK_SUCCESS) {
            return false;
        }

        _mapped = allocationInfo.pMappedData;
        if (_mapped == nullptr &&
            vmaMapMemory(_dev->Allocator(), _allocation, &_mapped) != VK_SUCCESS) {
            vmaDestroyBuffer(_dev->Allocator(), _buffer, _allocation);
            _buffer = VK_NULL_HANDLE;
            _allocation = VK_NULL_HANDLE;
            return false;
        }

        _writes.resize(_desc.capacity);
        return true;
    }

    bool VulkanResourceDescriptorHeap::ValidateRange(
        std::uint32_t firstIndex,
        std::uint32_t count) const
    {
        return firstIndex <= _desc.capacity &&
               count <= _desc.capacity - firstIndex;
    }

    void* VulkanResourceDescriptorHeap::GetSlotPtr(std::uint32_t index) const
    {
        return static_cast<std::uint8_t*>(_mapped) + index * _slotStride;
    }

    void VulkanResourceDescriptorHeap::FlushSlot(
        std::uint32_t index,
        VkDeviceSize size) const
    {
        (void)vmaFlushAllocation(
            _dev->Allocator(),
            _allocation,
            index * _slotStride,
            size);
    }

    VkDeviceAddress VulkanResourceDescriptorHeap::GetDeviceAddress() const
    {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = _buffer;
        return _dev->GetFnTable().vkGetBufferDeviceAddress(_dev->LogicalDev(), &addressInfo);
    }

    void VulkanResourceDescriptorHeap::Write(
        ResourceDescriptorIndex index,
        const ResourceDescriptorWrite& write)
    {
        if (!ValidateRange(index.value, 1)) {
            assert(false && "Resource descriptor write is out of heap range.");
            return;
        }

        std::visit(
            [this, slot = index.value](const auto& descriptor) {
                WriteDescriptor(slot, descriptor);
            },
            write);

        _writes[index.value] = write;
    }

    void VulkanResourceDescriptorHeap::WriteRange(
        ResourceDescriptorIndex firstIndex,
        std::span<const ResourceDescriptorWrite> writes)
    {
        if (!ValidateRange(firstIndex.value, static_cast<std::uint32_t>(writes.size()))) {
            assert(false && "Resource descriptor range write is out of heap range.");
            return;
        }

        for (std::uint32_t i = 0; i < writes.size(); ++i) {
            Write(firstIndex + i, writes[i]);
        }
    }

    void VulkanResourceDescriptorHeap::Clear(ResourceDescriptorIndex index)
    {
        if (!ValidateRange(index.value, 1)) {
            assert(false && "Resource descriptor clear is out of heap range.");
            return;
        }

        _writes[index.value] = ResourceDescriptorWrite{};
        // Vulkan clear intentionally leaves descriptor bytes untouched for now.
    }

    void VulkanResourceDescriptorHeap::ClearRange(
        ResourceDescriptorIndex firstIndex,
        std::uint32_t count)
    {
        if (!ValidateRange(firstIndex.value, count)) {
            assert(false && "Resource descriptor range clear is out of heap range.");
            return;
        }

        for (std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }

    void VulkanResourceDescriptorHeap::WriteDescriptor(
        std::uint32_t index,
        const UniformBufferDescriptor& write)
    {
        if (!write.buffer) {
            Clear(ResourceDescriptorIndex(index));
            return;
        }

        VkDescriptorAddressInfoEXT bufferInfo = GetBufferAddressInfo(_dev.get(), write.buffer);

        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorInfo.data.pUniformBuffer = &bufferInfo;

        const auto size = _dev->GetDevCaps().descriptorBufferProperties.uniformBufferDescriptorSize;
        _dev->GetFnTable().vkGetDescriptorEXT(
            _dev->LogicalDev(),
            &descriptorInfo,
            size,
            GetSlotPtr(index));
        FlushSlot(index, size);
    }

    void VulkanResourceDescriptorHeap::WriteDescriptor(
        std::uint32_t index,
        const ReadOnlyStorageBufferDescriptor& write)
    {
        if (!write.buffer) {
            Clear(ResourceDescriptorIndex(index));
            return;
        }

        VkDescriptorAddressInfoEXT bufferInfo = GetBufferAddressInfo(_dev.get(), write.buffer);

        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorInfo.data.pStorageBuffer = &bufferInfo;

        const auto size = _dev->GetDevCaps().descriptorBufferProperties.storageBufferDescriptorSize;
        _dev->GetFnTable().vkGetDescriptorEXT(
            _dev->LogicalDev(),
            &descriptorInfo,
            size,
            GetSlotPtr(index));
        FlushSlot(index, size);
    }

    void VulkanResourceDescriptorHeap::WriteDescriptor(
        std::uint32_t index,
        const ReadWriteStorageBufferDescriptor& write)
    {
        if (!write.buffer) {
            Clear(ResourceDescriptorIndex(index));
            return;
        }

        VkDescriptorAddressInfoEXT bufferInfo = GetBufferAddressInfo(_dev.get(), write.buffer);

        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorInfo.data.pStorageBuffer = &bufferInfo;

        const auto size = _dev->GetDevCaps().descriptorBufferProperties.storageBufferDescriptorSize;
        _dev->GetFnTable().vkGetDescriptorEXT(
            _dev->LogicalDev(),
            &descriptorInfo,
            size,
            GetSlotPtr(index));
        FlushSlot(index, size);
    }

    void VulkanResourceDescriptorHeap::WriteDescriptor(
        std::uint32_t index,
        const SampledTextureDescriptor& write)
    {
        if (!write.texture) {
            Clear(ResourceDescriptorIndex(index));
            return;
        }

        VkDescriptorImageInfo imageInfo =
            GetImageInfo(write.texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorInfo.data.pSampledImage = &imageInfo;

        const auto size = _dev->GetDevCaps().descriptorBufferProperties.sampledImageDescriptorSize;
        _dev->GetFnTable().vkGetDescriptorEXT(
            _dev->LogicalDev(),
            &descriptorInfo,
            size,
            GetSlotPtr(index));
        FlushSlot(index, size);
    }

    void VulkanResourceDescriptorHeap::WriteDescriptor(
        std::uint32_t index,
        const StorageTextureDescriptor& write)
    {
        if (!write.texture) {
            Clear(ResourceDescriptorIndex(index));
            return;
        }

        VkDescriptorImageInfo imageInfo =
            GetImageInfo(write.texture, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorInfo.data.pStorageImage = &imageInfo;

        const auto size = _dev->GetDevCaps().descriptorBufferProperties.storageImageDescriptorSize;
        _dev->GetFnTable().vkGetDescriptorEXT(
            _dev->LogicalDev(),
            &descriptorInfo,
            size,
            GetSlotPtr(index));
        FlushSlot(index, size);
    }

    VulkanSamplerDescriptorHeap::VulkanSamplerDescriptorHeap(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
    { }

    VulkanSamplerDescriptorHeap::~VulkanSamplerDescriptorHeap()
    {
        if (_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(_dev->Allocator(), _buffer, _allocation);
        }
    }

    common::sp<ISamplerDescriptorHeap> VulkanSamplerDescriptorHeap::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
    {
        if (dev->GetDevCaps().resourceBindingModel !=
            VulkanDevCaps::ResourceBindingModel::DescriptorBuffer) {
            return nullptr;
        }

        auto* heap = new VulkanSamplerDescriptorHeap(dev, desc);
        if (!heap->Init()) {
            delete heap;
            return nullptr;
        }

        return common::sp<ISamplerDescriptorHeap>(heap);
    }

    bool VulkanSamplerDescriptorHeap::Init()
    {
        if (_desc.capacity == 0) {
            assert(false && "Descriptor heap capacity must be greater than zero.");
            return false;
        }

        const auto& props = _dev->GetDevCaps().descriptorBufferProperties;
        _slotStride = AlignUp(
            props.samplerDescriptorSize,
            props.descriptorBufferOffsetAlignment);
        _sizeInBytes = _slotStride * _desc.capacity;

        if (_sizeInBytes > props.maxSamplerDescriptorBufferRange) {
            assert(false && "Sampler descriptor heap exceeds Vulkan descriptor buffer range.");
            return false;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = _sizeInBytes;
        bufferInfo.usage =
            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        allocInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo{};
        VkResult result = vmaCreateBuffer(
            _dev->Allocator(),
            &bufferInfo,
            &allocInfo,
            &_buffer,
            &_allocation,
            &allocationInfo);
        if (result != VK_SUCCESS) {
            return false;
        }

        _mapped = allocationInfo.pMappedData;
        if (_mapped == nullptr &&
            vmaMapMemory(_dev->Allocator(), _allocation, &_mapped) != VK_SUCCESS) {
            vmaDestroyBuffer(_dev->Allocator(), _buffer, _allocation);
            _buffer = VK_NULL_HANDLE;
            _allocation = VK_NULL_HANDLE;
            return false;
        }

        _writes.resize(_desc.capacity);
        return true;
    }

    bool VulkanSamplerDescriptorHeap::ValidateRange(
        std::uint32_t firstIndex,
        std::uint32_t count) const
    {
        return firstIndex <= _desc.capacity &&
               count <= _desc.capacity - firstIndex;
    }

    void* VulkanSamplerDescriptorHeap::GetSlotPtr(std::uint32_t index) const
    {
        return static_cast<std::uint8_t*>(_mapped) + index * _slotStride;
    }

    void VulkanSamplerDescriptorHeap::FlushSlot(
        std::uint32_t index,
        VkDeviceSize size) const
    {
        (void)vmaFlushAllocation(
            _dev->Allocator(),
            _allocation,
            index * _slotStride,
            size);
    }

    VkDeviceAddress VulkanSamplerDescriptorHeap::GetDeviceAddress() const
    {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = _buffer;
        return _dev->GetFnTable().vkGetBufferDeviceAddress(_dev->LogicalDev(), &addressInfo);
    }

    void VulkanSamplerDescriptorHeap::Write(
        SamplerDescriptorIndex index,
        const SamplerDescriptorWrite& sampler)
    {
        if (!ValidateRange(index.value, 1)) {
            assert(false && "Sampler descriptor write is out of heap range.");
            return;
        }

        if (!sampler) {
            Clear(index);
            return;
        }

        auto* vkSampler = common::PtrCast<VulkanSampler>(sampler.get());
        VkSampler rawSampler = vkSampler->GetHandle();

        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorInfo.data.pSampler = &rawSampler;

        const auto size = _dev->GetDevCaps().descriptorBufferProperties.samplerDescriptorSize;
        _dev->GetFnTable().vkGetDescriptorEXT(
            _dev->LogicalDev(),
            &descriptorInfo,
            size,
            GetSlotPtr(index.value));
        FlushSlot(index.value, size);

        _writes[index.value] = sampler;
    }

    void VulkanSamplerDescriptorHeap::WriteRange(
        SamplerDescriptorIndex firstIndex,
        std::span<const SamplerDescriptorWrite> samplers)
    {
        if (!ValidateRange(firstIndex.value, static_cast<std::uint32_t>(samplers.size()))) {
            assert(false && "Sampler descriptor range write is out of heap range.");
            return;
        }

        for (std::uint32_t i = 0; i < samplers.size(); ++i) {
            Write(firstIndex + i, samplers[i]);
        }
    }

    void VulkanSamplerDescriptorHeap::Clear(SamplerDescriptorIndex index)
    {
        if (!ValidateRange(index.value, 1)) {
            assert(false && "Sampler descriptor clear is out of heap range.");
            return;
        }

        _writes[index.value] = nullptr;
        // Vulkan clear intentionally leaves descriptor bytes untouched for now.
    }

    void VulkanSamplerDescriptorHeap::ClearRange(
        SamplerDescriptorIndex firstIndex,
        std::uint32_t count)
    {
        if (!ValidateRange(firstIndex.value, count)) {
            assert(false && "Sampler descriptor range clear is out of heap range.");
            return;
        }

        for (std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }
} // namespace alloy::vk
