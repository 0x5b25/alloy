#include "VulkanDescriptorHeap.hpp"

#include "alloy/common/Common.hpp"

#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace alloy::vk
{
    namespace
    {
        VkDeviceSize AlignUp(VkDeviceSize value, VkDeviceSize alignment) {
            if(alignment <= 1) {
                return value;
            }
            return ((value + alignment - 1) / alignment) * alignment;
        }

        void ValidateCapacity(std::uint32_t capacity) {
            if(capacity == 0) {
                throw std::invalid_argument("Descriptor heap capacity must be greater than zero.");
            }
        }

        void ValidateRange(
            std::uint32_t capacity,
            std::uint32_t first,
            std::uint32_t count) {
            if(count == 0) {
                if(first > capacity) {
                    throw std::out_of_range("Descriptor heap range starts past heap capacity.");
                }
                return;
            }
            if(first >= capacity || count > capacity - first) {
                throw std::out_of_range("Descriptor heap range exceeds heap capacity.");
            }
        }

        std::uint32_t CheckedSpanSize(std::size_t size) {
            if(size > std::numeric_limits<std::uint32_t>::max()) {
                throw std::out_of_range("Descriptor heap write range is too large.");
            }
            return static_cast<std::uint32_t>(size);
        }

        VkDeviceAddress GetBufferDeviceAddress(
            const common::sp<VulkanDevice>& dev,
            VkBuffer buffer) {
            VkBufferDeviceAddressInfo addressInfo {};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = buffer;

            const auto& fnTable = dev->GetFnTable();
            if(fnTable.vkGetBufferDeviceAddress != nullptr) {
                return fnTable.vkGetBufferDeviceAddress(dev->LogicalDev(), &addressInfo);
            }
            if(fnTable.vkGetBufferDeviceAddressKHR != nullptr) {
                return fnTable.vkGetBufferDeviceAddressKHR(dev->LogicalDev(), &addressInfo);
            }

            throw std::runtime_error("Vulkan buffer device address function is unavailable.");
        }

        void CreateDescriptorBuffer(
            const common::sp<VulkanDevice>& dev,
            VkDeviceSize size,
            VkBufferUsageFlags descriptorUsage,
            VkBuffer& buffer,
            VmaAllocation& allocation,
            void*& mappedData) {
            VkBufferCreateInfo bufferInfo {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = descriptorUsage |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

            VmaAllocationCreateInfo allocInfo {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            allocInfo.flags =
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo allocationInfo {};
            VkResult res = vmaCreateBuffer(
                dev->Allocator(),
                &bufferInfo,
                &allocInfo,
                &buffer,
                &allocation,
                &allocationInfo);
            if(res != VK_SUCCESS) {
                throw std::runtime_error("Failed to create Vulkan descriptor heap buffer.");
            }

            mappedData = allocationInfo.pMappedData;
            if(mappedData == nullptr) {
                vmaDestroyBuffer(dev->Allocator(), buffer, allocation);
                buffer = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
                throw std::runtime_error("Failed to map Vulkan descriptor heap buffer.");
            }
        }

        void FlushDescriptorWrite(
            const common::sp<VulkanDevice>& dev,
            VmaAllocation allocation,
            VkDeviceSize offset,
            VkDeviceSize size) {
            VK_CHECK(vmaFlushAllocation(dev->Allocator(), allocation, offset, size));
        }

        VkDeviceSize ResourceDescriptorSize(
            const VkPhysicalDeviceDescriptorBufferPropertiesEXT& props,
            VkDescriptorType type) {
            switch(type) {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    return props.uniformBufferDescriptorSize;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    return props.storageBufferDescriptorSize;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    return props.sampledImageDescriptorSize;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    return props.storageImageDescriptorSize;
                default:
                    throw std::invalid_argument("Unsupported resource descriptor heap write type.");
            }
        }

        VkDeviceSize MaxResourceDescriptorSize(
            const VkPhysicalDeviceDescriptorBufferPropertiesEXT& props) {
            return std::max({
                static_cast<VkDeviceSize>(props.uniformBufferDescriptorSize),
                static_cast<VkDeviceSize>(props.storageBufferDescriptorSize),
                static_cast<VkDeviceSize>(props.sampledImageDescriptorSize),
                static_cast<VkDeviceSize>(props.storageImageDescriptorSize),
            });
        }

        VkDeviceSize CheckedBufferSize(
            VkDeviceSize baseOffset,
            VkDeviceSize stride,
            std::uint32_t capacity
        ) {
            const VkDeviceSize maxSize = std::numeric_limits<VkDeviceSize>::max();
            if(stride != 0 && capacity > (maxSize - baseOffset) / stride) {
                throw std::invalid_argument("Descriptor heap size overflows VkDeviceSize.");
            }
            return baseOffset + stride * capacity;
        }

        VkDescriptorType ResourceDescriptorType(const UniformBufferDescriptor&) {
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }

        VkDescriptorType ResourceDescriptorType(const ReadOnlyStorageBufferDescriptor&) {
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        VkDescriptorType ResourceDescriptorType(const ReadWriteStorageBufferDescriptor&) {
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        VkDescriptorType ResourceDescriptorType(const SampledTextureDescriptor&) {
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }

        VkDescriptorType ResourceDescriptorType(const StorageTextureDescriptor&) {
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }

        constexpr std::array<VkDescriptorType, 4> ResourceHeapMutableDescriptorTypes = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        };

        struct HeapSetLayoutCreateInfo {
            VkDescriptorSetLayoutBinding binding {};
            VkDescriptorBindingFlags bindingFlags = 0;
            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo {};
            VkMutableDescriptorTypeListEXT mutableDescriptorTypeList {};
            VkMutableDescriptorTypeCreateInfoEXT mutableDescriptorTypeInfo {};
            VkDescriptorSetLayoutCreateInfo layoutInfo {};

            HeapSetLayoutCreateInfo(
                std::uint32_t bindingIndex,
                VkDescriptorType descriptorType,
                std::uint32_t descriptorCount,
                const VkDescriptorType* mutableDescriptorTypes,
                std::uint32_t mutableDescriptorTypeCount
            ) {
                binding.binding = bindingIndex;
                binding.descriptorType = descriptorType;
                binding.descriptorCount = descriptorCount;
                binding.stageFlags = VK_SHADER_STAGE_ALL;

                bindingFlags =
                    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

                bindingFlagsInfo.sType =
                    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                bindingFlagsInfo.bindingCount = 1;
                bindingFlagsInfo.pBindingFlags = &bindingFlags;

                layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layoutInfo.pNext = &bindingFlagsInfo;
                layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
                layoutInfo.bindingCount = 1;
                layoutInfo.pBindings = &binding;

                if(mutableDescriptorTypeCount > 0) {
                    mutableDescriptorTypeList.descriptorTypeCount =
                        mutableDescriptorTypeCount;
                    mutableDescriptorTypeList.pDescriptorTypes = mutableDescriptorTypes;

                    mutableDescriptorTypeInfo.sType =
                        VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT;
                    mutableDescriptorTypeInfo.pNext = layoutInfo.pNext;
                    mutableDescriptorTypeInfo.mutableDescriptorTypeListCount = 1;
                    mutableDescriptorTypeInfo.pMutableDescriptorTypeLists =
                        &mutableDescriptorTypeList;
                    layoutInfo.pNext = &mutableDescriptorTypeInfo;
                }
            }
        };

        std::uint32_t CountFromDescriptorBufferRange(
            VkDeviceSize range,
            VkDeviceSize stride) {
            if(stride == 0) {
                return 0;
            }

            const VkDeviceSize count = range / stride;
            return static_cast<std::uint32_t>(std::min<VkDeviceSize>(
                count,
                std::numeric_limits<std::uint32_t>::max()));
        }

        std::uint32_t ResourceHeapRequestedDescriptorCount(const VulkanDevCaps& caps) {
            const VkDeviceSize stride = AlignUp(
                MaxResourceDescriptorSize(caps.descriptorBufferProperties),
                caps.descriptorBufferProperties.descriptorBufferOffsetAlignment);

            return CountFromDescriptorBufferRange(
                caps.descriptorBufferProperties.maxResourceDescriptorBufferRange,
                stride);
        }

        std::uint32_t SamplerHeapRequestedDescriptorCount(const VulkanDevCaps& caps) {
            const VkDeviceSize stride = AlignUp(
                caps.descriptorBufferProperties.samplerDescriptorSize,
                caps.descriptorBufferProperties.descriptorBufferOffsetAlignment);

            return CountFromDescriptorBufferRange(
                caps.descriptorBufferProperties.maxSamplerDescriptorBufferRange,
                stride);
        }

        std::uint32_t QueryVariableDescriptorCount(
            VulkanDevice* dev,
            const HeapSetLayoutCreateInfo& layout
        ) {
            auto requestedDescriptorCount = layout.binding.descriptorCount;

            VkDescriptorSetVariableDescriptorCountLayoutSupport variableSupport {};
            variableSupport.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT;

            VkDescriptorSetLayoutSupport support {};
            support.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT;
            support.pNext = &variableSupport;

            VK_DEV_CALL(dev,
                vkGetDescriptorSetLayoutSupport(
                    dev->LogicalDev(),
                    &layout.layoutInfo,
                    &support));

            if(support.supported == VK_TRUE) {
                return requestedDescriptorCount;
            }

            VK_ASSERT(variableSupport.maxVariableDescriptorCount > 0);

            return std::min(
                requestedDescriptorCount,
                variableSupport.maxVariableDescriptorCount);
        }

        void DestroyDescriptorSetLayout(
            VulkanDevice* dev,
            VkDescriptorSetLayout& layout) {
            if(layout == VK_NULL_HANDLE) {
                return;
            }

            VK_DEV_CALL(dev,
                vkDestroyDescriptorSetLayout(dev->LogicalDev(), layout, nullptr));
            layout = VK_NULL_HANDLE;
        }


    }

    VulkanResourceDescriptorHeap::VulkanResourceDescriptorHeap(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
    {
        ValidateCapacity(_desc.capacity);

        const auto& caps = _dev->GetDevCaps();
        if(!caps.CanCreateExperimentalResourceDescriptorHeap()) {
            throw std::runtime_error(
                "Vulkan resource descriptor heaps require descriptor buffer and mutable descriptor type support.");
        }
        _layoutABI = _dev->GetDescriptorHeapLayoutABI();
        const VulkanDescriptorHeapSetLayout& layout = _layoutABI->ResourceLayout();
        ValidateLayoutCapacity(layout, _desc.capacity, "Resource");

        const auto& props = caps.descriptorBufferProperties;
        _stride = layout.descriptorStride;
        _bufferSize = CheckedBufferSize(layout.bindingOffset, _stride, _desc.capacity);

        if(_bufferSize > props.maxResourceDescriptorBufferRange) {
            throw std::invalid_argument("Resource descriptor heap exceeds Vulkan descriptor buffer range limit.");
        }

        CreateDescriptorBuffer(
            _dev,
            _bufferSize,
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
            _buffer,
            _allocation,
            _mappedData);
        _deviceAddress = GetBufferDeviceAddress(_dev, _buffer);
        _entries.resize(_desc.capacity);
    }

    VulkanResourceDescriptorHeap::~VulkanResourceDescriptorHeap() {
        if(_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(_dev->Allocator(), _buffer, _allocation);
        }

        DEBUGCODE(_buffer = VK_NULL_HANDLE);
        DEBUGCODE(_allocation = VK_NULL_HANDLE);
        DEBUGCODE(_mappedData = nullptr);
        DEBUGCODE(_deviceAddress = 0);
    }

    common::sp<IResourceDescriptorHeap> VulkanResourceDescriptorHeap::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc) {
        return common::sp<IResourceDescriptorHeap>(
            new VulkanResourceDescriptorHeap(dev, desc));
    }

    void VulkanResourceDescriptorHeap::EncodeDescriptor(
        ResourceDescriptorIndex index,
        const ResourceDescriptorWrite& write) {
        ValidateRange(_desc.capacity, index.value, 1);

        const VkDeviceSize offset = DescriptorOffset(_layoutABI->ResourceLayout(), index.value);
        auto* dst = static_cast<std::uint8_t*>(_mappedData) + offset;
        std::memset(dst, 0, static_cast<std::size_t>(_stride));

        VkDescriptorGetInfoEXT descriptorInfo {};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = std::visit(
            [](const auto& descriptor) {
                return ResourceDescriptorType(descriptor);
            },
            write);
        if(!_layoutABI->IsResourceDescriptorTypeAllowed(descriptorInfo.type)) {
            throw std::invalid_argument("Resource descriptor heap write uses an unsupported descriptor type.");
        }

        VkDescriptorAddressInfoEXT addressInfo {};
        addressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        addressInfo.format = VK_FORMAT_UNDEFINED;

        VkDescriptorImageInfo imageInfo {};

        std::visit(
            [&](const auto& descriptor) {
                using Descriptor = std::decay_t<decltype(descriptor)>;

                if constexpr(
                    std::is_same_v<Descriptor, UniformBufferDescriptor> ||
                    std::is_same_v<Descriptor, ReadOnlyStorageBufferDescriptor> ||
                    std::is_same_v<Descriptor, ReadWriteStorageBufferDescriptor>) {
                    if(!descriptor.buffer) {
                        throw std::invalid_argument("Buffer descriptor heap write contains a null buffer range.");
                    }

                    auto* vkBuffer =
                        common::PtrCast<VulkanBuffer>(descriptor.buffer->GetBufferObject());

                    addressInfo.address =
                        GetBufferDeviceAddress(_dev, vkBuffer->GetHandle()) +
                        descriptor.buffer->GetShape().GetOffsetInBytes();
                    addressInfo.range = descriptor.buffer->GetShape().GetSizeInBytes();

                    if constexpr(std::is_same_v<Descriptor, UniformBufferDescriptor>) {
                        descriptorInfo.data.pUniformBuffer = &addressInfo;
                    } else {
                        descriptorInfo.data.pStorageBuffer = &addressInfo;
                    }
                } else if constexpr(
                    std::is_same_v<Descriptor, SampledTextureDescriptor> ||
                    std::is_same_v<Descriptor, StorageTextureDescriptor>) {
                    if(!descriptor.texture) {
                        throw std::invalid_argument("Texture descriptor heap write contains a null texture view.");
                    }

                    auto* vkTexView =
                        common::PtrCast<VulkanTextureView>(descriptor.texture.get());
                    imageInfo.imageView = vkTexView->GetHandle();
                    imageInfo.imageLayout =
                        std::is_same_v<Descriptor, StorageTextureDescriptor>
                            ? VK_IMAGE_LAYOUT_GENERAL
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    if constexpr(std::is_same_v<Descriptor, StorageTextureDescriptor>) {
                        descriptorInfo.data.pStorageImage = &imageInfo;
                    } else {
                        descriptorInfo.data.pSampledImage = &imageInfo;
                    }
                }
            },
            write);

        const VkDeviceSize descriptorSize =
            ResourceDescriptorSize(_dev->GetDevCaps().descriptorBufferProperties, descriptorInfo.type);
        VK_DEV_CALL(_dev,
            vkGetDescriptorEXT(
                _dev->LogicalDev(),
                &descriptorInfo,
                descriptorSize,
                dst));

        FlushDescriptorWrite(_dev, _allocation, offset, _stride);
    }

    void VulkanResourceDescriptorHeap::ClearDescriptor(ResourceDescriptorIndex index) {
        ValidateRange(_desc.capacity, index.value, 1);

        const VkDeviceSize offset = DescriptorOffset(_layoutABI->ResourceLayout(), index.value);
        auto* dst = static_cast<std::uint8_t*>(_mappedData) + offset;
        std::memset(dst, 0, static_cast<std::size_t>(_stride));
        FlushDescriptorWrite(_dev, _allocation, offset, _stride);
        _entries[index.value].reset();
    }

    void VulkanResourceDescriptorHeap::Write(
        ResourceDescriptorIndex index,
        const ResourceDescriptorWrite& write) {
        EncodeDescriptor(index, write);
        _entries[index.value] = write;
    }

    void VulkanResourceDescriptorHeap::WriteRange(
        ResourceDescriptorIndex firstIndex,
        std::span<const ResourceDescriptorWrite> writes) {
        const std::uint32_t count = CheckedSpanSize(writes.size());
        ValidateRange(_desc.capacity, firstIndex.value, count);

        for(std::uint32_t i = 0; i < count; ++i) {
            Write(firstIndex + i, writes[i]);
        }
    }

    void VulkanResourceDescriptorHeap::Clear(ResourceDescriptorIndex index) {
        ClearDescriptor(index);
    }

    void VulkanResourceDescriptorHeap::ClearRange(
        ResourceDescriptorIndex firstIndex,
        std::uint32_t count) {
        ValidateRange(_desc.capacity, firstIndex.value, count);

        for(std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }

    VulkanSamplerDescriptorHeap::VulkanSamplerDescriptorHeap(
        const common::sp<VulkanDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
    {
        ValidateCapacity(_desc.capacity);

        const auto& caps = _dev->GetDevCaps();
        if(!caps.CanCreateExperimentalSamplerDescriptorHeap()) {
            throw std::runtime_error(
                "Vulkan sampler descriptor heaps require descriptor buffer support.");
        }
        _layoutABI = _dev->GetDescriptorHeapLayoutABI();
        const VulkanDescriptorHeapSetLayout& layout = _layoutABI->SamplerLayout();
        ValidateLayoutCapacity(layout, _desc.capacity, "Sampler");

        const auto& props = caps.descriptorBufferProperties;
        _stride = layout.descriptorStride;
        _bufferSize = CheckedBufferSize(layout.bindingOffset, _stride, _desc.capacity);

        if(_bufferSize > props.maxSamplerDescriptorBufferRange) {
            throw std::invalid_argument("Sampler descriptor heap exceeds Vulkan descriptor buffer range limit.");
        }

        CreateDescriptorBuffer(
            _dev,
            _bufferSize,
            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            _buffer,
            _allocation,
            _mappedData);
        _deviceAddress = GetBufferDeviceAddress(_dev, _buffer);
        _entries.resize(_desc.capacity);
    }

    VulkanSamplerDescriptorHeap::~VulkanSamplerDescriptorHeap() {
        if(_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(_dev->Allocator(), _buffer, _allocation);
        }

        DEBUGCODE(_buffer = VK_NULL_HANDLE);
        DEBUGCODE(_allocation = VK_NULL_HANDLE);
        DEBUGCODE(_mappedData = nullptr);
        DEBUGCODE(_deviceAddress = 0);
    }

    common::sp<ISamplerDescriptorHeap> VulkanSamplerDescriptorHeap::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc) {
        return common::sp<ISamplerDescriptorHeap>(
            new VulkanSamplerDescriptorHeap(dev, desc));
    }

    void VulkanSamplerDescriptorHeap::EncodeDescriptor(
        SamplerDescriptorIndex index,
        const SamplerDescriptorWrite& sampler) {
        ValidateRange(_desc.capacity, index.value, 1);
        if(!sampler) {
            throw std::invalid_argument("Sampler descriptor heap write contains a null sampler.");
        }

        const VkDeviceSize offset = DescriptorOffset(_layoutABI->SamplerLayout(), index.value);
        auto* dst = static_cast<std::uint8_t*>(_mappedData) + offset;
        std::memset(dst, 0, static_cast<std::size_t>(_stride));

        auto* vkSampler = common::PtrCast<VulkanSampler>(sampler.get());
        VkSampler samplerHandle = vkSampler->GetHandle();

        VkDescriptorGetInfoEXT descriptorInfo {};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorInfo.data.pSampler = &samplerHandle;

        VK_DEV_CALL(_dev,
            vkGetDescriptorEXT(
                _dev->LogicalDev(),
                &descriptorInfo,
                _dev->GetDevCaps().descriptorBufferProperties.samplerDescriptorSize,
                dst));

        FlushDescriptorWrite(_dev, _allocation, offset, _stride);
    }

    void VulkanSamplerDescriptorHeap::ClearDescriptor(SamplerDescriptorIndex index) {
        ValidateRange(_desc.capacity, index.value, 1);

        const VkDeviceSize offset = DescriptorOffset(_layoutABI->SamplerLayout(), index.value);
        auto* dst = static_cast<std::uint8_t*>(_mappedData) + offset;
        std::memset(dst, 0, static_cast<std::size_t>(_stride));
        FlushDescriptorWrite(_dev, _allocation, offset, _stride);
        _entries[index.value] = nullptr;
    }

    void VulkanSamplerDescriptorHeap::Write(
        SamplerDescriptorIndex index,
        const SamplerDescriptorWrite& sampler) {
        EncodeDescriptor(index, sampler);
        _entries[index.value] = sampler;
    }

    void VulkanSamplerDescriptorHeap::WriteRange(
        SamplerDescriptorIndex firstIndex,
        std::span<const SamplerDescriptorWrite> samplers) {
        const std::uint32_t count = CheckedSpanSize(samplers.size());
        ValidateRange(_desc.capacity, firstIndex.value, count);

        for(std::uint32_t i = 0; i < count; ++i) {
            Write(firstIndex + i, samplers[i]);
        }
    }

    void VulkanSamplerDescriptorHeap::Clear(SamplerDescriptorIndex index) {
        ClearDescriptor(index);
    }

    void VulkanSamplerDescriptorHeap::ClearRange(
        SamplerDescriptorIndex firstIndex,
        std::uint32_t count) {
        ValidateRange(_desc.capacity, firstIndex.value, count);

        for(std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }
}
