#pragma once

#include <memory>
#include <string>
#include <vector>

#include <volk.h>
#include <vk_mem_alloc.h>

#include "common/error.h"
#include "common/macros.h"

class Buffer;
class SwapChain;
class Semaphore;
class Fence;

class Device : public std::enable_shared_from_this<Device>{
    friend class Buffer;
    friend class SwapChain;
    friend class Semaphore;
    friend class Fence;

    DISALLOW_COPY_AND_ASSIGN(Device);

private:
    //External
    VkInstance _instance;
    VkSurfaceKHR _surface;

    //Managed
    VkPhysicalDevice _phyDev;
    VkDevice _dev;

    VkQueue _queueGraphics, _queueCopy, _queueCompute;

    bool _hasComputeCap;
    bool _hasUniqueCpyQueue, _hasUniqueComputeQueue;
    bool _isValid = false;

    VmaAllocator _allocator;
    VkCommandPool _cmdPool;

public:

    Device() = default;

    ~Device();

    static std::shared_ptr<Device> Make(
        VkInstance instance, 
        VkSurfaceKHR surface, 
        const std::vector<std::string>& extensions = {}
    );

    bool IsValid() const {return _isValid;}
    const VkDevice& Handle() const {return _dev;}
    const VkPhysicalDevice& GPU() const {return _phyDev;}

    const VkSurfaceKHR& Surface() const {return _surface;}

    const VkQueue& GraphicsQueue() const {return _queueGraphics;}

    const VkCommandPool& CmdPool() const { return _cmdPool; }
    const VmaAllocator& Allocator() const {return _allocator;}

//
    std::shared_ptr<Buffer> AllocateBuffer(
        std::uint64_t size,
        VkBufferUsageFlags usages,
        VmaMemoryUsage allocationType = VMA_MEMORY_USAGE_GPU_ONLY,
        VkMemoryPropertyFlags requiredProperties = 0,
        VkMemoryPropertyFlags preferredProperties = 0
    );
};

class Buffer{
    friend class Device;

    DISALLOW_COPY_AND_ASSIGN(Buffer);
private:
    std::shared_ptr<Device> _dev;

    VkBuffer _buffer;
    std::uint64_t _size;
    VmaAllocation _allocation;

    VkBufferUsageFlags _usages;
    VmaMemoryUsage _allocationType;

    bool _isValid = false;

public:
    Buffer() = default;
    ~Buffer();

    VkBuffer Handle() const{return _buffer;}

    std::uint64_t Size() const {return _size;}

    void* MapToCPU()
    {
        void* mappedData;
        auto res = vmaMapMemory(_dev->Allocator(), _allocation, &mappedData);
        if (res != VK_SUCCESS) return nullptr;
        return mappedData;
    }

    void UnMap()
    {
        vmaUnmapMemory(_dev->Allocator(), _allocation);
    }

};

class SwapChain{
    DISALLOW_COPY_AND_ASSIGN(SwapChain);
private:
    std::shared_ptr<Device> _dev;

    VkSwapchainKHR _swapChain;
    VkExtent2D _extent;
    VkSurfaceFormatKHR _format;
    VkPresentModeKHR _presentMode;
    uint32_t _imgCnt;

    std::vector<VkImage> _images;
    std::vector<VkImageView> _imageViews;
    std::vector<VkFramebuffer> _framebuffers;

    bool _isValid = false;

public:
    SwapChain() = default;
    ~SwapChain();

    static std::shared_ptr<SwapChain> Make(std::shared_ptr<Device>& device);

    bool IsValid() const {return _isValid;}
    const VkSwapchainKHR& Handle() const {return _swapChain;}

    size_t ImageCount() const { return _imgCnt; }

    const std::vector<VkImageView>& ImageViews()const { return _imageViews; }
    const VkExtent2D& Extent() const { return _extent; }
    const VkSurfaceFormatKHR& Format() const { return _format; }


};

class Semaphore
{
    DISALLOW_COPY_AND_ASSIGN(Semaphore);
private:
    std::shared_ptr<Device> _dev;
    VkSemaphore _semaphore;

    bool _isValid = false;

public:
    Semaphore() = default;
    ~Semaphore();

    static std::shared_ptr<Semaphore> Make(std::shared_ptr<Device>& device);

    bool IsValid() const { return _isValid; }
    const VkSemaphore& Handle() const { return _semaphore; }
};

class Fence
{
    DISALLOW_COPY_AND_ASSIGN(Fence);
private:
    std::shared_ptr<Device> _dev;
    VkFence _fence;

    bool _isValid = false;

public:
    Fence() = default;
    ~Fence();

    static std::shared_ptr<Fence> Make(std::shared_ptr<Device>& device, bool signaled = false);

    bool IsValid() const { return _isValid; }
    const VkFence& Handle() const { return _fence; }

    bool IsSignaled() const
    {
        auto res = vkGetFenceStatus(_dev->Handle(), _fence);
        VK_ASSERT(res != VK_ERROR_DEVICE_LOST);
        return res == VK_SUCCESS;
    }

};
