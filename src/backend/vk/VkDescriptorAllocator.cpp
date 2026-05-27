#include "VkDescriptorAllocator.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "alloy/common/Common.hpp"
#include "VulkanDevice.hpp"
#include "VulkanBindableResource.hpp"

namespace alloy::vk
{
    namespace
    {

#pragma region DescPoolAlloc

        std::vector<VkDescriptorPoolSize> DefaultPoolSizes(uint32_t maxSets) {
            return {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1u * maxSets },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4u * maxSets },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4u * maxSets },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u * maxSets },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1u * maxSets },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1u * maxSets },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2u * maxSets },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u * maxSets },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1u * maxSets },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1u * maxSets },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u * maxSets },
            };
        }

        VkDescriptorGetInfoEXT MakeDescriptorGetInfo(
            VkDescriptorType type,
            const VkDescriptorImageInfo* imageInfo,
            const VkDescriptorAddressInfoEXT* addressInfo,
            const VkDeviceAddress* accelerationStructure)
        {
            VkDescriptorGetInfoEXT info {};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
            info.type = type;

            switch(type) {
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    if(imageInfo == nullptr) {
                        throw std::invalid_argument("Sampler descriptor write requires imageInfos.");
                    }
                    info.data.pSampler = &imageInfo->sampler;
                    break;

                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    if(imageInfo == nullptr) {
                        throw std::invalid_argument("Combined image sampler descriptor write requires imageInfos.");
                    }
                    info.data.pCombinedImageSampler = imageInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    if(imageInfo == nullptr) {
                        throw std::invalid_argument("Sampled image descriptor write requires imageInfos.");
                    }
                    info.data.pSampledImage = imageInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    if(imageInfo == nullptr) {
                        throw std::invalid_argument("Storage image descriptor write requires imageInfos.");
                    }
                    info.data.pStorageImage = imageInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                    if(imageInfo == nullptr) {
                        throw std::invalid_argument("Input attachment descriptor write requires imageInfos.");
                    }
                    info.data.pInputAttachmentImage = imageInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    if(addressInfo == nullptr) {
                        throw std::invalid_argument("Uniform texel buffer descriptor write requires addressInfos.");
                    }
                    info.data.pUniformTexelBuffer = addressInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    if(addressInfo == nullptr) {
                        throw std::invalid_argument("Storage texel buffer descriptor write requires addressInfos.");
                    }
                    info.data.pStorageTexelBuffer = addressInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    if(addressInfo == nullptr) {
                        throw std::invalid_argument("Uniform buffer descriptor write requires addressInfos.");
                    }
                    info.data.pUniformBuffer = addressInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    if(addressInfo == nullptr) {
                        throw std::invalid_argument("Storage buffer descriptor write requires addressInfos.");
                    }
                    info.data.pStorageBuffer = addressInfo;
                    break;

                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    if(accelerationStructure == nullptr) {
                        throw std::invalid_argument("Acceleration structure descriptor write requires accelerationStructures.");
                    }
                    info.data.accelerationStructure = *accelerationStructure;
                    break;

                default:
                    throw std::invalid_argument("Unsupported descriptor type for descriptor-buffer write.");
            }

            return info;
        }
    }

    struct VkDescriptorPoolAllocator::PoolPage {
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;

        ~PoolPage() {
            if(pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, pool, nullptr);
            }
        }
    };

    class PoolDescriptorSet final : public IVkDescriptorSet {
    public:
        PoolDescriptorSet(
            VkDevice device,
            VkDescriptorSetLayout layout,
            VkDescriptorSet set,
            std::shared_ptr<VkDescriptorPoolAllocator::PoolPage> page)
            : _device(device)
            , _layout(layout)
            , _set(set)
            , _page(std::move(page))
        {
        }

        VkDescriptorSetLayout GetLayout() const override {
            return _layout;
        }

        VkDescriptorSet GetDescriptorSet() const override {
            return _set;
        }

        void Write(std::span<const VkDescriptorWriteOp> writes) override {
            std::vector<VkWriteDescriptorSet> vkWrites;
            vkWrites.reserve(writes.size());

            for(const auto& write : writes) {
                if(write.descriptorCount == 0) {
                    continue;
                }

                VkWriteDescriptorSet vkWrite {};
                vkWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                vkWrite.dstSet = _set;
                vkWrite.dstBinding = write.binding;
                vkWrite.dstArrayElement = write.firstArrayElement;
                vkWrite.descriptorType = write.descriptorType;
                vkWrite.descriptorCount = write.descriptorCount;
                vkWrite.pImageInfo = write.imageInfos;
                vkWrite.pBufferInfo = write.bufferInfos;
                vkWrite.pTexelBufferView = write.texelBufferViews;

                vkWrites.push_back(vkWrite);
            }

            if(!vkWrites.empty()) {
                vkUpdateDescriptorSets(
                    _device,
                    static_cast<uint32_t>(vkWrites.size()),
                    vkWrites.data(),
                    0,
                    nullptr);
            }
        }

        void Bind(const VkDescriptorBindArgs& args) const override {
            vkCmdBindDescriptorSets(
                args.commandBuffer,
                args.pipelineBindPoint,
                args.pipelineLayout,
                args.setIndex,
                1,
                &_set,
                0,
                nullptr);
        }

    private:
        VkDevice _device = VK_NULL_HANDLE;
        VkDescriptorSetLayout _layout = VK_NULL_HANDLE;
        VkDescriptorSet _set = VK_NULL_HANDLE;
        std::shared_ptr<VkDescriptorPoolAllocator::PoolPage> _page;
    };

    VkDescriptorPoolAllocator::VkDescriptorPoolAllocator(const CreateArgs& args)
        : _args(args)
    {
        if(_args.maxSetsPerPool == 0) {
            throw std::invalid_argument("VkDescriptorPoolAllocator maxSetsPerPool must be greater than zero.");
        }
        if(_args.poolSizes.empty()) {
            _args.poolSizes = DefaultPoolSizes(_args.maxSetsPerPool);
        }
    }

    VkDescriptorPoolAllocator::~VkDescriptorPoolAllocator() = default;

    
    VkDevice VkDescriptorPoolAllocator::GetDev() const {
        return _args.device->LogicalDev();
    }

    VkPhysicalDevice VkDescriptorPoolAllocator::GetPhyDev() const {
        return _args.device->PhysicalDev();
    }

    std::shared_ptr<VkDescriptorPoolAllocator::PoolPage>
    VkDescriptorPoolAllocator::CreatePoolPage() {
        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = _args.poolFlags;
        poolInfo.maxSets = _args.maxSetsPerPool;
        poolInfo.poolSizeCount = static_cast<uint32_t>(_args.poolSizes.size());
        poolInfo.pPoolSizes = _args.poolSizes.data();

        VkDescriptorPool pool = VK_NULL_HANDLE;
        VK_CHECK(VK_DEV_CALL(_args.device,
            vkCreateDescriptorPool(GetDev(), &poolInfo, nullptr, &pool)),
            "Failed to create Vulkan descriptor pool.");

        auto page = std::make_shared<PoolPage>();
        page->device = GetDev();
        page->pool = pool;
        _pages.push_back(page);
        return page;
    }

    std::unique_ptr<IVkDescriptorSet> VkDescriptorPoolAllocator::Allocate(
        const VkDescriptorSetAllocateArgs& args)
    {
        std::scoped_lock lock(_mutex);

        if(!_currentPage) {
            _currentPage = CreatePoolPage();
        }

        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &args.layout;

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo {};
        uint32_t variableCount = 0;

        if(args.variableDescriptorCount.has_value()) {
            variableCount = args.variableDescriptorCount->descriptorCount;
            variableCountInfo.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variableCountInfo.descriptorSetCount = 1;
            variableCountInfo.pDescriptorCounts = &variableCount;
            allocInfo.pNext = &variableCountInfo;
        }


        VkDescriptorSet set = VK_NULL_HANDLE;
        allocInfo.descriptorPool = _currentPage->pool;

        VkResult result = VK_DEV_CALL(_args.device,
            vkAllocateDescriptorSets(GetDev(), &allocInfo, &set));
        if(result == VK_ERROR_FRAGMENTED_POOL || result == VK_ERROR_OUT_OF_POOL_MEMORY) {
            _currentPage = CreatePoolPage();
            allocInfo.descriptorPool = _currentPage->pool;
            result = VK_DEV_CALL(
                _args.device,
                vkAllocateDescriptorSets(GetDev(), &allocInfo, &set));
        }

        VK_ASSERT(result == VK_SUCCESS, "Failed to allocate Vulkan descriptor set.");

        return std::make_unique<PoolDescriptorSet>(
            _args.device,
            args.layout,
            set,
            _currentPage);
    }

#pragma endregion

#pragma region DescBufferAlloc

    struct VkDescriptorBufferAllocator::BufferPage {
        VulkanDevice* device = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceAddress deviceAddress = 0;
        VkDeviceSize size = 0;
        VkDeviceSize cursor = 0;
        VkBufferUsageFlags usage = 0;

        ~BufferPage() {
            if(mapped != nullptr) {
                VK_DEV_CALL(device, vkUnmapMemory(device->LogicalDev(), memory));
            }
            if(buffer != VK_NULL_HANDLE) {
                VK_DEV_CALL(device, vkDestroyBuffer(device->LogicalDev(), buffer, nullptr));
            }
            if(memory != VK_NULL_HANDLE) {
                VK_DEV_CALL(device, vkFreeMemory(device->LogicalDev(), memory, nullptr));
            }
        }
    };

    class BufferDescriptorSet final : public IVkDescriptorSet {
    public:
        BufferDescriptorSet(
            VkDevice device,
            VkPhysicalDeviceDescriptorBufferPropertiesEXT properties,
            VkDescriptorSetLayout layout,
            std::shared_ptr<VkDescriptorBufferAllocator::BufferPage> page,
            VkDeviceSize offset,
            VkDeviceSize size)
            : _device(device)
            , _properties(properties)
            , _layout(layout)
            , _page(std::move(page))
            , _offset(offset)
            , _size(size)
        {
        }

        VkDescriptorSetLayout GetLayout() const override {
            return _layout;
        }

        void Write(std::span<const VkDescriptorWriteOp> writes) override {
            for(const auto& write : writes) {
                if(write.descriptorCount == 0) {
                    continue;
                }

                VkDeviceSize bindingOffset = 0;
                vkGetDescriptorSetLayoutBindingOffsetEXT(
                    _device,
                    _layout,
                    write.binding,
                    &bindingOffset);

                const VkDeviceSize stride = write.descriptorStride != 0
                    ? write.descriptorStride
                    : DescriptorSize(write.descriptorType);
                const VkDeviceSize descriptorSize = DescriptorSize(write.descriptorType);

                for(uint32_t i = 0; i < write.descriptorCount; ++i) {
                    VkDescriptorAddressInfoEXT addressInfo {};
                    addressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
                    addressInfo.format = VK_FORMAT_UNDEFINED;

                    const VkDescriptorImageInfo* imageInfo = write.imageInfos != nullptr
                        ? &write.imageInfos[i]
                        : nullptr;
                    const VkDescriptorAddressInfoEXT* addressInfoPtr = write.addressInfos != nullptr
                        ? &write.addressInfos[i]
                        : nullptr;
                    const VkDeviceAddress* accelerationStructure =
                        write.accelerationStructures != nullptr
                            ? &write.accelerationStructures[i]
                            : nullptr;

                    if(addressInfoPtr == nullptr && write.bufferInfos != nullptr) {
                        VkBufferDeviceAddressInfo addressQuery {};
                        addressQuery.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                        addressQuery.buffer = write.bufferInfos[i].buffer;

                        addressInfo.address =
                            vkGetBufferDeviceAddress(_device, &addressQuery) +
                            write.bufferInfos[i].offset;
                        addressInfo.range = write.bufferInfos[i].range;
                        addressInfoPtr = &addressInfo;
                    }

                    auto getInfo = MakeDescriptorGetInfo(
                        write.descriptorType,
                        imageInfo,
                        addressInfoPtr,
                        accelerationStructure);

                    const VkDeviceSize descriptorOffset =
                        _offset +
                        bindingOffset +
                        stride * (write.firstArrayElement + i);
                    if(descriptorOffset + descriptorSize > _offset + _size) {
                        throw std::out_of_range("Descriptor-buffer write exceeds allocated descriptor set storage.");
                    }

                    auto* dst =
                        static_cast<uint8_t*>(_page->mapped) + descriptorOffset;
                    std::memset(dst, 0, static_cast<size_t>(stride));

                    vkGetDescriptorEXT(
                        _device,
                        &getInfo,
                        descriptorSize,
                        dst);
                }
            }
        }

        void Bind(const VkDescriptorBindArgs& args) const override {
            if(args.bindDescriptorBuffer) {
                if(args.descriptorBufferIndex != 0) {
                    throw std::invalid_argument(
                        "Descriptor-buffer self-bind only supports descriptorBufferIndex 0.");
                }

                VkDescriptorBufferBindingInfoEXT bindingInfo {};
                bindingInfo.sType =
                    VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
                bindingInfo.address = _page->deviceAddress;
                bindingInfo.usage = _page->usage;

                vkCmdBindDescriptorBuffersEXT(
                    args.commandBuffer,
                    1,
                    &bindingInfo);
            }

            const uint32_t bufferIndex = args.descriptorBufferIndex;
            const VkDeviceSize offset = _offset;

            vkCmdSetDescriptorBufferOffsetsEXT(
                args.commandBuffer,
                args.pipelineBindPoint,
                args.pipelineLayout,
                args.setIndex,
                1,
                &bufferIndex,
                &offset);
        }

    private:
        VkDeviceSize DescriptorSize(VkDescriptorType type) const {
            switch(type) {
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    return _properties.samplerDescriptorSize;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    return _properties.combinedImageSamplerDescriptorSize;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    return _properties.sampledImageDescriptorSize;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    return _properties.storageImageDescriptorSize;
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    return _properties.uniformTexelBufferDescriptorSize;
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    return _properties.storageTexelBufferDescriptorSize;
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    return _properties.uniformBufferDescriptorSize;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    return _properties.storageBufferDescriptorSize;
                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                    return _properties.inputAttachmentDescriptorSize;
                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    return _properties.accelerationStructureDescriptorSize;
                default:
                    throw std::invalid_argument("Unsupported descriptor type for descriptor-buffer sizing.");
            }
        }

        VkDevice _device = VK_NULL_HANDLE;
        VkPhysicalDeviceDescriptorBufferPropertiesEXT _properties {};
        VkDescriptorSetLayout _layout = VK_NULL_HANDLE;
        std::shared_ptr<VkDescriptorBufferAllocator::BufferPage> _page;
        VkDeviceSize _offset = 0;
        VkDeviceSize _size = 0;
    };

    VkDescriptorBufferAllocator::VkDescriptorBufferAllocator(const CreateArgs& args)
        : _args(args)
    {
        if(_args.pageSize == 0) {
            throw std::invalid_argument("VkDescriptorBufferAllocator pageSize must be greater than zero.");
        }
        if(_args.allocationAlignment == 0) {
            _args.allocationAlignment =
                std::max<VkDeviceSize>(
                    1,
                    _args.descriptorBufferProperties.descriptorBufferOffsetAlignment);
        }

        _memTypeIdx = _QuerySupportedMemoryType();
    }

    VkDescriptorBufferAllocator::~VkDescriptorBufferAllocator() = default;

    
    
    VkDevice VkDescriptorBufferAllocator::GetDev() const {
        return _args.device->LogicalDev();
    }

    VkPhysicalDevice VkDescriptorBufferAllocator::GetPhyDev() const {
        return _args.device->PhysicalDev();
    }


    uint32_t VkDescriptorBufferAllocator::_QuerySupportedMemoryType() {

        //Create a "dummy buffer" to query supported memory types
        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = _args.pageSize;
        bufferInfo.usage =
            _args.descriptorBufferUsage |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VkBuffer buffer = VK_NULL_HANDLE;
        VK_CHECK(VK_DEV_CALL(_args.device,
            vkCreateBuffer(GetDev(), &bufferInfo, nullptr, &buffer)),
            "Failed to create Vulkan descriptor buffer.");

        VkMemoryRequirements requirements {};
        VK_DEV_CALL(_args.device,
            vkGetBufferMemoryRequirements(GetDev(), buffer, &requirements));
        
        auto allowedMemTypeBits = requirements.memoryTypeBits;

        VK_DEV_CALL(_args.device,
            vkDestroyBuffer(GetDev(), buffer, nullptr));
        
        VkPhysicalDeviceMemoryProperties properties {};
        VK_INST_CALL(_args.device,
            vkGetPhysicalDeviceMemoryProperties(GetPhyDev(), &properties));

        const auto requiredFlags = _args.memoryPropertyFlags;

        for(uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            const bool typeAllowed = (allowedMemTypeBits & (1u << i)) != 0;
            const bool flagsMatch =
                (properties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags;
            if(typeAllowed && flagsMatch) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find a compatible Vulkan memory type.");
    }

    VkDeviceSize VkDescriptorBufferAllocator::DescriptorSize(
        VkDescriptorType type) const
    {
        switch(type) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                return _args.descriptorBufferProperties.samplerDescriptorSize;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                return _args.descriptorBufferProperties.combinedImageSamplerDescriptorSize;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return _args.descriptorBufferProperties.sampledImageDescriptorSize;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                return _args.descriptorBufferProperties.storageImageDescriptorSize;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                return _args.descriptorBufferProperties.uniformTexelBufferDescriptorSize;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                return _args.descriptorBufferProperties.storageTexelBufferDescriptorSize;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                return _args.descriptorBufferProperties.uniformBufferDescriptorSize;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                return _args.descriptorBufferProperties.storageBufferDescriptorSize;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                return _args.descriptorBufferProperties.inputAttachmentDescriptorSize;
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                return _args.descriptorBufferProperties.accelerationStructureDescriptorSize;
            default:
                throw std::invalid_argument("Unsupported descriptor type for descriptor-buffer sizing.");
        }
    }

    VkDeviceSize VkDescriptorBufferAllocator::DescriptorStride(
        VkDescriptorType type) const
    {
        return common::AlignUp(DescriptorSize(type), _args.allocationAlignment);
    }

    VkDescriptorBufferAllocator::BufferPage*
    VkDescriptorBufferAllocator::_CreateBufferPage(VkDeviceSize minSize) {
        const VkDeviceSize pageSize = std::max(_args.pageSize, minSize);

        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = pageSize;
        bufferInfo.usage =
            _args.descriptorBufferUsage |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VkBuffer buffer = VK_NULL_HANDLE;
        VK_CHECK(VK_DEV_CALL(_args.device,
            vkCreateBuffer(GetDev(), &bufferInfo, nullptr, &buffer)),
            "Failed to create Vulkan descriptor buffer.");

        VkMemoryRequirements requirements {};
        VK_DEV_CALL(_args.device,
            vkGetBufferMemoryRequirements(GetDev(), buffer, &requirements));


        VkMemoryAllocateFlagsInfo allocateFlags {};
        allocateFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocateFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext = &allocateFlags;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex = _memTypeIdx;

        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkResult result = VK_DEV_CALL(_args.device, vkAllocateMemory(
            GetDev(),
            &allocateInfo,
            nullptr,
            &memory));
        if(result != VK_SUCCESS) {
            VK_DEV_CALL(_args.device, vkDestroyBuffer(GetDev(), buffer, nullptr));
            VK_CHECK(false, "Failed to allocate Vulkan descriptor buffer memory.");
        }

        result = VK_DEV_CALL(_args.device, 
            vkBindBufferMemory(GetDev(), buffer, memory, 0));
        if(result != VK_SUCCESS) {
            VK_DEV_CALL(_args.device, vkFreeMemory(GetDev(), memory, nullptr));
            VK_DEV_CALL(_args.device, vkDestroyBuffer(GetDev(), buffer, nullptr));
            CheckVk(result, "Failed to bind Vulkan descriptor buffer memory.");
        }

        void* mapped = nullptr;
        result = VK_DEV_CALL(_args.device,
            vkMapMemory(GetDev(), memory, 0, pageSize, 0, &mapped));
        if(result != VK_SUCCESS) {
            VK_DEV_CALL(_args.device, vkFreeMemory(GetDev(), memory, nullptr));
            VK_DEV_CALL(_args.device, vkDestroyBuffer(GetDev(), buffer, nullptr));
            CheckVk(result, "Failed to map Vulkan descriptor buffer memory.");
        }

        VkBufferDeviceAddressInfo addressInfo {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer;

        auto page = std::make_unique<BufferPage>();
        page->device = _args.device;
        page->buffer = buffer;
        page->memory = memory;
        page->mapped = mapped;
        page->deviceAddress =
            VK_DEV_CALL(_args.device, vkGetBufferDeviceAddress(GetDev(), &addressInfo));
        page->size = pageSize;
        page->cursor = 0;
        page->usage = _args.descriptorBufferUsage;

        _pages.push_back(page);
        return page.get();
    }

    
    VkDescriptorBufferAllocator::BufferPage*
    VkDescriptorBufferAllocator::_GetOrCreateBufferPage(VkDeviceSize minSize) {

    }

    std::unique_ptr<IVkDescriptorSet> VkDescriptorBufferAllocator::Allocate(
        const VkDescriptorSetAllocateArgs& args)
    {

        VkDeviceSize layoutSize = 0;
        vkGetDescriptorSetLayoutSizeEXT(_args.device, args.layout, &layoutSize);

        VkDeviceSize allocationSize = layoutSize;
        if(args.variableDescriptorCount.has_value() &&
           args.variableDescriptorCount->descriptorStride != 0)
        {
            VkDeviceSize bindingOffset = 0;
            vkGetDescriptorSetLayoutBindingOffsetEXT(
                _args.device,
                args.layout,
                args.variableDescriptorCount->binding,
                &bindingOffset);

            allocationSize =
                bindingOffset +
                args.variableDescriptorCount->descriptorStride *
                    args.variableDescriptorCount->descriptorCount;
        }

        allocationSize = common::AlignUp(allocationSize, _args.allocationAlignment);
        if(allocationSize == 0) {
            throw std::invalid_argument("Descriptor-buffer allocation size must be greater than zero.");
        }

        std::scoped_lock lock(_mutex);

        if(!_currentPage) {
            _currentPage = CreateBufferPage(allocationSize);
        }

        VkDeviceSize alignedCursor =
            AlignUp(_currentPage->cursor, _args.allocationAlignment);
        if(alignedCursor + allocationSize > _currentPage->size) {
            _currentPage = CreateBufferPage(allocationSize);
            alignedCursor = 0;
        }

        _currentPage->cursor = alignedCursor + allocationSize;

        return std::make_unique<BufferDescriptorSet>(
            _args.device,
            _args.descriptorBufferProperties,
            args.layout,
            _currentPage,
            alignedCursor,
            allocationSize);
    }
#pragma endregion
}
