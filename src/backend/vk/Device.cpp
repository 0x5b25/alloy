#define VMA_IMPLEMENTATION
#include "Device.hpp"

#include <vector>
#include <string>
#include <tuple>
#include <map>
#include <cstdint>

#include "common/optional.h"
#include "common/error.h"
#include "common/helpers.h"

struct PhyDevInfo{
    std::string name;
    VkPhysicalDevice handle;
    uint32_t graphicsQueueFamily; bool graphicsQueueSupportsCompute;
    vkb::Optional<uint32_t> computeQueueFamily;
    vkb::Optional<uint32_t> transferQueueFamily;
};

vkb::Optional<PhyDevInfo> _PickPhysicalDevice(
    VkInstance inst, 
    VkSurfaceKHR surface,
    const std::vector<std::string>& extensions
){
    std::uint32_t devCnt;
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &devCnt, nullptr));
    std::vector<VkPhysicalDevice> gpus(devCnt);
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &devCnt, gpus.data()));

    //std::vector<VkPhysicalDevice> gpus =  context.instance.enumeratePhysicalDevices();
    std::vector<std::tuple<std::uint32_t, PhyDevInfo>> devList;

    auto _RatePhyDev = [&](VkPhysicalDevice phyDev, std::uint32_t& score, PhyDevInfo& info){
        //Can device do geomerty shaders?
        VkPhysicalDeviceFeatures devFeat{};
        vkGetPhysicalDeviceFeatures(phyDev, &devFeat);
        if(!devFeat.geometryShader) return false;

        //Does device supports required extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(phyDev, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(phyDev, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        if(!requiredExtensions.empty()) return false;

        //Find graphics queue
        std::uint32_t queueFamCnt;
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamCnt, nullptr);
		std::vector<VkQueueFamilyProperties> queue_family_properties(queueFamCnt);
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamCnt, queue_family_properties.data());
        
		if (queue_family_properties.empty())
		{
			return false;
		}

        auto _FindUniqueFamilyWithFlags = [&](uint32_t flags, const std::vector<uint32_t>& excludes){
            for (uint32_t j = 0; j < vkb::to_u32(queue_family_properties.size()); j++)
		    {
                bool shouldExclude = false;
                for(auto& f : excludes) if(f == j) {shouldExclude = true; break;}

                if(shouldExclude) continue;

                if (queue_family_properties[j].queueFlags & flags)return vkb::Optional<uint32_t>(j);
		    }
            return vkb::Optional<uint32_t>();
        };

        std::vector<uint32_t> excludes;

        //Lets find a graphics queue first
        {
            while (true)
            {
                //uint32_t gQueue;
                auto res = _FindUniqueFamilyWithFlags(
                    VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT, 
                    excludes);
                if(!res.has_value()){
                    //No Graphics queue means no suitable device
                    return false;
                }
                //Add to exclude list
                excludes.push_back(res.value());
                //Check for presentation support
                VkBool32 supports_present;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(phyDev, res.value(), surface, &supports_present));
			    // Find a queue family which supports graphics and presentation.
			    if (supports_present)
			    {
			    	info.graphicsQueueFamily = res.value();
			    	break;
			    }
            }
        }

        
        VkPhysicalDeviceProperties devProp{};
        vkGetPhysicalDeviceProperties(phyDev, &devProp);
        info.name = devProp.deviceName;
        info.handle = phyDev;

        //Score the device
        score = 0;
        {
            auto& gQueueProp = queue_family_properties[info.graphicsQueueFamily];
            if(gQueueProp.queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT){
                info.graphicsQueueSupportsCompute = true;
                score += 100;
            }
        }
        if(devProp.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;

        {
            //Try find dedicated compute queue
            auto res = _FindUniqueFamilyWithFlags(VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT, excludes);
            if(res.has_value()){
                excludes.push_back(res.value());
                info.computeQueueFamily = res.value();
                score += 200;
            }
        }

        {
            //Try find dedicated transfer queue
            auto res = _FindUniqueFamilyWithFlags(VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT, excludes);
            if(res.has_value()){
                excludes.push_back(res.value());
                info.transferQueueFamily = res.value();
                score += 100;
            }
        }
        //Evaluate vram size
        VkPhysicalDeviceMemoryProperties vramProp{};
        vkGetPhysicalDeviceMemoryProperties(phyDev, &vramProp);

        for(uint32_t i = 0; i < vramProp.memoryHeapCount; i++){
            //Locate the first device local heap
            if(vramProp.memoryHeaps[i].flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT){
                score += vramProp.memoryHeaps[i].size / (8 * 1024 * 1024);
                break;
            }
        }

        return true;
    };

    for(auto& gpu : gpus){
        std::uint32_t score; PhyDevInfo info{};
        if(!_RatePhyDev(gpu, score, info)){
            continue;
        }
        devList.push_back({score, info});
    }

    if(devList.empty()) return {};
    auto max = 0, maxScore = 0;
    for(auto i = 0; i < devList.size(); i++){
        auto score = std::get<uint32_t>(devList[i]);
        if(score >= maxScore) max = i, maxScore = score;
    }
    return std::get<PhyDevInfo>(devList[max]);
}

std::shared_ptr<Device> Device::Make(
    VkInstance instance, 
    VkSurfaceKHR surface, 
    const std::vector<std::string>& extensions
){

    auto res = _PickPhysicalDevice(instance, surface, extensions);
    if(!res.has_value()) return nullptr;

    auto& devInfo = res.value();

    auto dev = std::make_shared<Device>();
    dev->_instance = instance;
    dev->_surface = surface;
    dev->_phyDev = res.value().handle;
    dev->_hasComputeCap = res.value().graphicsQueueSupportsCompute;

    
    //Create logical device
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
    std::vector<float> queuePriorities{};
    {
        queueCreateInfos.emplace_back();
        VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = devInfo.graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;

        queuePriorities.push_back(1.0f);
    }

    if(devInfo.transferQueueFamily.has_value()){
        queueCreateInfos.emplace_back();
        VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = devInfo.transferQueueFamily.value();
        queueCreateInfo.queueCount = 1;

        queuePriorities.push_back(0.8f);

        dev->_hasUniqueCpyQueue = true;
    }

    if(devInfo.computeQueueFamily.has_value()){
        queueCreateInfos.emplace_back();
        VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = devInfo.computeQueueFamily.value();
        queueCreateInfo.queueCount = 1;

        queuePriorities.push_back(0.6f);

        dev->_hasUniqueComputeQueue = true;
    }

    for(auto i = 0; i < queueCreateInfos.size(); i++){
        queueCreateInfos[i].pQueuePriorities = &queuePriorities[i];
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    VkPhysicalDeviceFeatures deviceFeatures{};

    //First add pointers to the queue creation info and device features structs:

    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();

    //Requested device extensions
    std::vector<const char*> extension_c{extensions.size()};
    for(auto i = 0; i < extensions.size(); i++) extension_c[i] = extensions[i].c_str();

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extension_c.size());
    createInfo.ppEnabledExtensionNames = extension_c.data();

    createInfo.pEnabledFeatures = &deviceFeatures;
    VK_CHECK(vkCreateDevice(dev->_phyDev, &createInfo, nullptr, &dev->_dev));

    //Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = devInfo.graphicsQueueFamily;
    poolInfo.flags = 0; // Optional
    VK_CHECK(vkCreateCommandPool(dev->_dev, &poolInfo, nullptr, &dev->_cmdPool));

    //Get queues
    vkGetDeviceQueue(dev->_dev, devInfo.graphicsQueueFamily, 0, &dev->_queueGraphics);
    if(devInfo.transferQueueFamily.has_value())
        vkGetDeviceQueue(dev->_dev, devInfo.transferQueueFamily.value(), 0, &dev->_queueCopy);
    if(devInfo.computeQueueFamily.has_value())
        vkGetDeviceQueue(dev->_dev, devInfo.computeQueueFamily.value(), 0, &dev->_queueCompute);

    //Init allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
    allocatorInfo.physicalDevice = dev->GPU();
    allocatorInfo.device = dev->Handle();
    allocatorInfo.instance = instance;

    VmaVulkanFunctions fn;
    fn.vkAllocateMemory = (PFN_vkAllocateMemory)vkAllocateMemory;
    fn.vkBindBufferMemory = (PFN_vkBindBufferMemory)vkBindBufferMemory;
    fn.vkBindImageMemory = (PFN_vkBindImageMemory)vkBindImageMemory;
    fn.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)vkCmdCopyBuffer;
    fn.vkCreateBuffer = (PFN_vkCreateBuffer)vkCreateBuffer;
    fn.vkCreateImage = (PFN_vkCreateImage)vkCreateImage;
    fn.vkDestroyBuffer = (PFN_vkDestroyBuffer)vkDestroyBuffer;
    fn.vkDestroyImage = (PFN_vkDestroyImage)vkDestroyImage;
    fn.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vkFlushMappedMemoryRanges;
    fn.vkFreeMemory = (PFN_vkFreeMemory)vkFreeMemory;
    fn.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vkGetBufferMemoryRequirements;
    fn.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)vkGetImageMemoryRequirements;
    fn.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetPhysicalDeviceMemoryProperties;
    fn.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetPhysicalDeviceProperties;
    fn.vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)vkInvalidateMappedMemoryRanges;
    fn.vkMapMemory = (PFN_vkMapMemory)vkMapMemory;
    fn.vkUnmapMemory = (PFN_vkUnmapMemory)vkUnmapMemory;
    fn.vkGetBufferMemoryRequirements2KHR = 0;  //(PFN_vkGetBufferMemoryRequirements2KHR)vkGetBufferMemoryRequirements2KHR;
    fn.vkGetImageMemoryRequirements2KHR = 0;  //(PFN_vkGetImageMemoryRequirements2KHR)vkGetImageMemoryRequirements2KHR;
    allocatorInfo.pVulkanFunctions = &fn;

    vmaCreateAllocator(&allocatorInfo, &dev->_allocator);

    dev->_isValid = true;

    return dev;

}

Device::~Device(){
    vmaDestroyAllocator(_allocator);
    vkDestroyCommandPool(_dev, _cmdPool, nullptr);
    vkDestroyDevice(_dev, nullptr);
}

std::shared_ptr<Buffer> Device::AllocateBuffer(
    std::uint64_t size,
    VkBufferUsageFlags usages,
    VmaMemoryUsage allocationType,
    VkMemoryPropertyFlags requiredProperties,
    VkMemoryPropertyFlags preferredProperties
){
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usages;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = allocationType;
    allocInfo.requiredFlags = requiredProperties;
    allocInfo.preferredFlags = preferredProperties;
    
    VkBuffer buffer;
    VmaAllocation allocation;
    auto res = vmaCreateBuffer(_allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

    if(res != VK_SUCCESS) return nullptr;

    auto buf = std::make_shared<Buffer>();
    buf->_dev = shared_from_this();
    buf->_buffer = buffer;
    buf->_size = size;
    buf->_allocation = allocation;

    buf->_usages = usages;
    buf->_allocationType = allocationType;

    buf->_isValid = true;

    return buf;
}

Buffer::~Buffer(){
    vmaDestroyBuffer(_dev->_allocator, _buffer, _allocation);
}

VkSurfaceFormatKHR _ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

//VkExtent2D _ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
//    if (capabilities.currentExtent.width != UINT32_MAX) {
//        return capabilities.currentExtent;
//    } else {
//        int width, height;
//        glfwGetFramebufferSize(window, &width, &height);
//
//        VkExtent2D actualExtent = {
//            static_cast<uint32_t>(width),
//            static_cast<uint32_t>(height)
//        };
//
//        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
//        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
//
//        return actualExtent;
//    }
//}

std::shared_ptr<SwapChain> SwapChain::Make(std::shared_ptr<Device>& device){
    auto dev = device->GPU();
    auto surface = device->Surface();

    //Query swapchain capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats{formatCount};
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes{presentModeCount};
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount, presentModes.data());

    if(formatCount <= 0 || presentModeCount <= 0) return nullptr;

    auto swapChain = std::make_shared<SwapChain>();

    //Choose format and present modes
    swapChain->_format = _ChooseSurfaceFormat(formats);
    swapChain->_presentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
    swapChain->_extent = capabilities.currentExtent.width != UINT32_MAX?
        capabilities.currentExtent : capabilities.maxImageExtent;
    swapChain->_imgCnt = capabilities.minImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = swapChain->_imgCnt;
    createInfo.imageFormat = swapChain->_format.format;
    createInfo.imageColorSpace = swapChain->_format.colorSpace;
    createInfo.imageExtent = swapChain->_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = swapChain->_presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device->Handle(), &createInfo, nullptr, &swapChain->_swapChain));

    swapChain->_dev = device;

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device->Handle(), swapChain->Handle(), &imageCount, nullptr);
    swapChain->_images.resize(imageCount);
    vkGetSwapchainImagesKHR(
        device->Handle(), swapChain->Handle(), 
        &imageCount, swapChain->_images.data());

    //Create image views
    swapChain->_imageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChain->_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChain->_format.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device->Handle(), &createInfo, nullptr, &swapChain->_imageViews[i]));
    }

    

    swapChain->_isValid = true;

    return swapChain;
}

Semaphore::~Semaphore()
{
    vkDestroySemaphore(_dev->Handle(), _semaphore, nullptr);
}

std::shared_ptr<Semaphore> Semaphore::Make(std::shared_ptr<Device>& device)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore raw_sem;
    VK_CHECK(vkCreateSemaphore(device->Handle(), &semaphoreInfo, nullptr, &raw_sem));

    auto sem = std::make_shared<Semaphore>();
    sem->_dev = device;
    sem->_semaphore = raw_sem;
    sem->_isValid = true;

    return sem;
}

Fence::~Fence()
{
    vkDestroyFence(_dev->Handle(), _fence, nullptr);
}

std::shared_ptr<Fence> Fence::Make(std::shared_ptr<Device>& device, bool signaled)
{
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if(signaled)
    {
        fenceCreateInfo.flags |= VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
    }
    VkFence raw_fence;
    VK_CHECK(vkCreateFence(device->Handle(), &fenceCreateInfo, nullptr, &raw_fence));

    auto fen = std::make_shared<Fence>();
    fen->_dev = device;
    fen->_fence = raw_fence;
    fen->_isValid = true;

    return fen;
}

SwapChain::~SwapChain(){
    for (auto& imageView : _imageViews) {
        vkDestroyImageView(_dev->Handle(), imageView, nullptr);
    }
    vkDestroySwapchainKHR(_dev->Handle(), _swapChain, nullptr);
}
