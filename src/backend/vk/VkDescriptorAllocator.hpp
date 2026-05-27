#pragma once

#include <volk.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

// The descriptor allocation backend. Thread safe allocation and destruction.
// 2 allocating modes:
//   1. Full descriptor layout support:
//       - allocate all the entries described in layout
//       - allows mixed descriptor types within the same layout
//   2. Variable binding count:
//       - allocate entries "up to" given count
//       - allocate all the entries described in layout

namespace alloy::vk
{
    class VulkanDevice;
    class VulkanResourceLayout;

    struct VkDescriptorWriteOp {
        uint32_t binding = 0;
        uint32_t firstArrayElement = 0;
        VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        uint32_t descriptorCount = 0;

        const VkDescriptorImageInfo* imageInfos = nullptr;
        const VkDescriptorBufferInfo* bufferInfos = nullptr;
        const VkBufferView* texelBufferViews = nullptr;

        // Used by the descriptor-buffer backend for texel-buffer descriptors.
        const VkDescriptorAddressInfoEXT* addressInfos = nullptr;

        // Used by the descriptor-buffer backend for acceleration structures.
        const VkDeviceAddress* accelerationStructures = nullptr;

        // Optional stride override for mutable descriptor bindings. If zero,
        // the descriptor-buffer backend derives stride from descriptorType.
        VkDeviceSize descriptorStride = 0;
    };

    struct VkDescriptorBindArgs {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        uint32_t setIndex = 0;

        // Descriptor-buffer backend only. If bindDescriptorBuffer is true,
        // descriptorBufferIndex must be 0 because vkCmdBindDescriptorBuffersEXT
        // binds a compact array starting at index 0.
        uint32_t descriptorBufferIndex = 0;
        bool bindDescriptorBuffer = true;
    };

    class IVkDescriptorSet {
    public:
        virtual ~IVkDescriptorSet() = default;

        virtual VkDescriptorSetLayout GetLayout() const = 0;
        virtual VkDescriptorSet GetDescriptorSet() const { return VK_NULL_HANDLE; }

        virtual void Write(std::span<const VkDescriptorWriteOp> writes) = 0;
        virtual void Bind(const VkDescriptorBindArgs& args) const = 0;
    };

    struct VkDescriptorSetAllocateArgs {
        VkDescriptorSetLayout layout;

        struct VariableDescriptorCount {
            uint32_t binding;
            uint32_t descriptorCount;

            // Descriptor-buffer backend only. If zero, the backend allocates
            // the full size reported by vkGetDescriptorSetLayoutSizeEXT.
            //VkDeviceSize descriptorStride = 0;
        };

        std::optional<VariableDescriptorCount> variableDescriptorCount;
    };

    class IVkDescriptorAllocator {
    public:
        virtual ~IVkDescriptorAllocator() = default;
        virtual std::unique_ptr<IVkDescriptorSet> Allocate(
            const VkDescriptorSetAllocateArgs& args) = 0;
    };

    class VkDescriptorPoolAllocator final : public IVkDescriptorAllocator {
    public:
        struct PoolPage;

        struct CreateArgs {
            VulkanDevice* device;

            uint32_t maxSetsPerPool = 1024;
            VkDescriptorPoolCreateFlags poolFlags = 0;

            // If empty, a conservative default pool mix is used.
            std::vector<VkDescriptorPoolSize> poolSizes;
        };

        explicit VkDescriptorPoolAllocator(const CreateArgs& args);
        ~VkDescriptorPoolAllocator() override;

        VkDescriptorPoolAllocator(const VkDescriptorPoolAllocator&) = delete;
        VkDescriptorPoolAllocator& operator=(const VkDescriptorPoolAllocator&) = delete;

        std::unique_ptr<IVkDescriptorSet> Allocate(
            const VkDescriptorSetAllocateArgs& args) override;

    private:
        std::shared_ptr<PoolPage> CreatePoolPage();

        CreateArgs _args;
        std::mutex _mutex;
        std::vector<std::shared_ptr<PoolPage>> _pages;
        std::shared_ptr<PoolPage> _currentPage;

        VkDevice GetDev() const;
        VkPhysicalDevice GetPhyDev() const;
    };

    class VkDescriptorBufferAllocator final : public IVkDescriptorAllocator {
    public:
        struct BufferPage;

        struct CreateArgs {
            VulkanDevice* device;

            VkDeviceSize pageSize = 2u * 1024u * 1024u; // Normally OS lage page is 2MB
            VkDeviceSize allocationAlignment = 1;

        //Separate sampler and resource desc.
            VkBufferUsageFlags descriptorBufferUsage =
                VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;

            VkMemoryPropertyFlags memoryPropertyFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties {};
        };

        explicit VkDescriptorBufferAllocator(const CreateArgs& args);
        ~VkDescriptorBufferAllocator() override;

        VkDescriptorBufferAllocator(const VkDescriptorBufferAllocator&) = delete;
        VkDescriptorBufferAllocator& operator=(const VkDescriptorBufferAllocator&) = delete;

        std::unique_ptr<IVkDescriptorSet> Allocate(
            const VkDescriptorSetAllocateArgs& args) override;

    private:
        BufferPage* _CreateBufferPage(VkDeviceSize minSize);
        BufferPage* _GetOrCreateBufferPage(VkDeviceSize minSize);
        VkDeviceSize DescriptorSize(VkDescriptorType type) const;
        VkDeviceSize DescriptorStride(VkDescriptorType type) const;

        CreateArgs _args;
        uint32_t _memTypeIdx;
        std::mutex _mutex;
        std::vector<std::unique_ptr<BufferPage>> _pages;
        BufferPage* _currentPage;

        uint32_t _QuerySupportedMemoryType();

        VkDevice GetDev() const;
        VkPhysicalDevice GetPhyDev() const;
    };
}
