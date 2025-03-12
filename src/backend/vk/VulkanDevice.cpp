#define VMA_IMPLEMENTATION

#include "VulkanDevice.hpp"

#include "alloy/common/Common.hpp"
#include "alloy/common/RefCnt.hpp"
#include "alloy/backend/Backends.hpp"

#include <atomic>
#include <mutex>
#include <cassert>
#include <optional>
#include <vector>
#include <unordered_set>
#include <format>
#include <iostream>

#include "VkSurfaceUtil.hpp"
#include "VkCommon.hpp"
#include "VulkanContext.hpp"
#include "VulkanCommandList.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanResourceBarrier.hpp"

namespace alloy::vk {

template<typename T, typename U>
std::unordered_set<T> ToSet(U&& obj) {
    return std::unordered_set<T>(obj.begin(), obj.end());
}

template<typename T, typename U>
bool Contains(T&& container, U&& element) {

    return container.find(element) != container.end();
}

    VulkanDevice::VulkanDevice() {};

    VulkanDevice::~VulkanDevice()
    {
        _fnTable.vkDeviceWaitIdle(_dev);

        
        delete _gfxQ;
        delete _copyQ;
        delete _computeQ;

        //if(_isOwnSurface){
        //    vkDestroySurfaceKHR(_ctx->GetHandle(), _surface, nullptr);
        //}
        vmaDestroyAllocator(_allocator);

        //Uninit managers
        _descPoolMgr.DeInit();
        //_cmdPoolMgr.DeInit();

        _fnTable.vkDestroyDevice(_dev, nullptr);

        DEBUGCODE(_adp = nullptr);
        DEBUGCODE(_dev = VK_NULL_HANDLE);
        DEBUGCODE(_allocator = VK_NULL_HANDLE);

        DEBUGCODE(_gfxQ = VK_NULL_HANDLE);
        DEBUGCODE(_copyQ = VK_NULL_HANDLE);
        DEBUGCODE(_computeQ = VK_NULL_HANDLE);

        //DEBUGCODE(_surface = VK_NULL_HANDLE);
    }

    common::sp<IGraphicsDevice> VulkanDevice::Make(
		const common::sp<VulkanAdapter>& adp,
		const IGraphicsDevice::Options& options
	){
        auto ctx = adp->GetCtx();

        //Make the surface if possible
        //VK::priv::SurfaceContainer _surf = {VK_NULL_HANDLE, false};

        //if (swapChainSource != nullptr) {
        //    _surf = VK::priv::CreateSurface(ctx->GetHandle(), swapChainSource);
        //}

        //auto phyDev = _PickPhysicalDevice(ctx->GetHandle(), nullptr/*_surf.surface*/, {});
        //if (!phyDev.has_value()) {
        //    return {};
        //}

        auto& qInfo = adp->GetQueueFamilyInfo();

        auto dev = common::sp<VulkanDevice>(new VulkanDevice());
        dev->_adp = adp;
        //ev->_surface = _surf.surface;
        //dev->_isOwnSurface = _surf.isOwnSurface;
        //dev->_phyDev = devInfo;
        dev->_features = {};
        dev->_features.hasComputeCap = qInfo.graphicsQueueSupportsCompute;

        //Create logical device
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
        std::vector<float> queuePriorities{};
        {
            queueCreateInfos.emplace_back();
            VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = qInfo.graphicsQueueFamily;
            queueCreateInfo.queueCount = 1;

            queuePriorities.push_back(1.0f);
        }

        if (qInfo.transferQueueFamily.has_value()) {
            queueCreateInfos.emplace_back();
            VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = qInfo.transferQueueFamily.value();
            queueCreateInfo.queueCount = 1;

            queuePriorities.push_back(0.8f);

            dev->_features.hasUniqueCpyQueue = true;
        }

        if (qInfo.computeQueueFamily.has_value()) {
            queueCreateInfos.emplace_back();
            VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = qInfo.computeQueueFamily.value();
            queueCreateInfo.queueCount = 1;

            queuePriorities.push_back(0.6f);

            dev->_features.hasUniqueComputeQueue = true;
        }

        for (auto i = 0; i < queueCreateInfos.size(); i++) {
            queueCreateInfos[i].pQueuePriorities = &queuePriorities[i];
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        VkPhysicalDeviceFeatures deviceFeatures{};
        vkGetPhysicalDeviceFeatures(adp->GetHandle(), &deviceFeatures);

        VkPhysicalDeviceVulkan12Features features12 { };
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.timelineSemaphore = true;
        createInfo.pNext = &features12;

        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature { };
        dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        dynamicRenderingFeature.dynamicRendering = VK_TRUE,
        features12.pNext = &dynamicRenderingFeature;

        VkPhysicalDeviceSynchronization2Features sync2Features{};
        sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        sync2Features.synchronization2 = VK_TRUE;
        dynamicRenderingFeature.pNext = &sync2Features;

        //First add pointers to the queue creation info and device features structs:

        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = queueCreateInfos.size();

        //Requested device extensions
        //First, get all available extensions
        std::unordered_set<std::string> availableDevExts{};
        {

            std::uint32_t propCount = 0;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(adp->GetHandle(), nullptr, &propCount, nullptr));

            if (propCount != 0) {
                std::vector<VkExtensionProperties> props(propCount);
                VK_CHECK(vkEnumerateDeviceExtensionProperties(
                    adp->GetHandle(), nullptr, &propCount, props.data()));

                
                for (int i = 0; i < propCount; i++)
                {
                    auto& prop = props[i];
                    availableDevExts.emplace(prop.extensionName);
                }
            }
        }

        std::vector<const char*> devExtensions{};
        auto _AddExtIfPresent = [&](const char* extName) {
            if (Contains(availableDevExts, extName))
            {
                devExtensions.push_back(extName);
                return true;
            }
            return false;
        };

        if(!_AddExtIfPresent(VkDevExtNames::VK_KHR_TIMELINE_SEMAPHORE)) {
            throw std::exception("VkDevice creation failed. Timeline semaphore not supported");
        }

        if(!_AddExtIfPresent(VkDevExtNames::VK_KHR_DYNAMIC_RENDERING)) {
            throw std::exception("VkDevice creation failed. dynamic rendering not supported");
        }

        if(!_AddExtIfPresent(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
            throw std::exception("VkDevice creation failed. synchronization2 not supported");
        }


        if (options.debug)
            dev->_features.supportsDebug = _AddExtIfPresent(VkDevExtNames::VK_EXT_DEBUG_MARKER);

        dev->_features.supportsPresent = _AddExtIfPresent(VkDevExtNames::VK_KHR_SWAPCHAIN);
        if (options.preferStandardClipSpaceYDirection) {
            dev->_features.supportsMaintenance1 = _AddExtIfPresent(VkDevExtNames::VK_KHR_MAINTENANCE1);
        }
        dev->_features.supportsGetMemReq2 = _AddExtIfPresent(VkDevExtNames::VK_KHR_GET_MEMORY_REQ2);
        dev->_features.supportsDedicatedAlloc = _AddExtIfPresent(VkDevExtNames::VK_KHR_DEDICATED_ALLOCATION);

        if (ctx->GetFeatures().hasDrvProp2Ext) {
            dev->_features.supportsDrvPropQuery = _AddExtIfPresent(VkDevExtNames::VK_KHR_DRIVER_PROPS);
        }

        dev->_features.supportsDepthClip = _AddExtIfPresent(VkDevExtNames::VK_EXT_DEPTH_CLIP_ENABLE);

        createInfo.enabledExtensionCount = static_cast<uint32_t>(devExtensions.size());
        createInfo.ppEnabledExtensionNames = devExtensions.data();

        createInfo.pEnabledFeatures = &deviceFeatures;
        VK_CHECK(vkCreateDevice(adp->GetHandle(), &createInfo, nullptr, &dev->_dev));

        volkLoadDeviceTable(&dev->_fnTable, dev->_dev);

        //Create command pool
        //VkCommandPoolCreateInfo poolInfo{};
        //poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        //poolInfo.queueFamilyIndex = devInfo.graphicsQueueFamily;
        //poolInfo.flags = 0; // Optional
        //VK_CHECK(vkCreateCommandPool(dev->_dev, &poolInfo, nullptr, &dev->_cmdPool));
        //dev->_cmdPoolMgr.Init(dev->_dev, devInfo.graphicsQueueFamily);
        dev->_descPoolMgr.Init(dev.get(), 1000);

        //Get queues
        {
            VkQueue rawGfxQ;
            dev->_fnTable.vkGetDeviceQueue(dev->_dev, qInfo.graphicsQueueFamily, 0, &rawGfxQ);
            dev->_gfxQ = new VulkanCommandQueue(dev.get(), qInfo.graphicsQueueFamily, rawGfxQ);
        }
        if (qInfo.transferQueueFamily.has_value()) {
            VkQueue rawCopyQ;
            dev->_fnTable.vkGetDeviceQueue(dev->_dev, qInfo.transferQueueFamily.value(), 0, &rawCopyQ);
            dev->_copyQ = new VulkanCommandQueue(dev.get(),
                                                 qInfo.transferQueueFamily.value(),
                                                 rawCopyQ);
        }
        if (qInfo.computeQueueFamily.has_value()) {
            VkQueue rawComputeQ;
            dev->_fnTable.vkGetDeviceQueue(dev->_dev, qInfo.computeQueueFamily.value(), 0, &rawComputeQ);
            dev->_computeQ = new VulkanCommandQueue(dev.get(),
                                                    qInfo.computeQueueFamily.value(),
                                                    rawComputeQ);
        }

        //Init allocator
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        allocatorInfo.physicalDevice = adp->GetHandle();
        allocatorInfo.device = dev->_dev;
        allocatorInfo.instance = adp->GetCtx()->GetHandle();

        VmaVulkanFunctions fn{};
        fn.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        fn.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        fn.vkAllocateMemory = dev->_fnTable.vkAllocateMemory;
        fn.vkBindBufferMemory = dev->_fnTable.vkBindBufferMemory;
        fn.vkBindImageMemory = dev->_fnTable.vkBindImageMemory;
        fn.vkCmdCopyBuffer = dev->_fnTable.vkCmdCopyBuffer;
        fn.vkCreateBuffer = dev->_fnTable.vkCreateBuffer;
        fn.vkCreateImage = dev->_fnTable.vkCreateImage;
        fn.vkDestroyBuffer = dev->_fnTable.vkDestroyBuffer;
        fn.vkDestroyImage = dev->_fnTable.vkDestroyImage;
        fn.vkFlushMappedMemoryRanges = dev->_fnTable.vkFlushMappedMemoryRanges;
        fn.vkFreeMemory = dev->_fnTable.vkFreeMemory;
        fn.vkGetBufferMemoryRequirements = dev->_fnTable.vkGetBufferMemoryRequirements;
        fn.vkGetImageMemoryRequirements = dev->_fnTable.vkGetImageMemoryRequirements;
        fn.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        fn.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        fn.vkInvalidateMappedMemoryRanges = dev->_fnTable.vkInvalidateMappedMemoryRanges;
        fn.vkMapMemory = dev->_fnTable.vkMapMemory;
        fn.vkUnmapMemory = dev->_fnTable.vkUnmapMemory;
        if (dev->_features.supportsGetMemReq2 && dev->_features.supportsDedicatedAlloc) {
            fn.vkGetBufferMemoryRequirements2KHR 
                = dev->_fnTable.vkGetBufferMemoryRequirements2KHR;
            fn.vkGetImageMemoryRequirements2KHR 
                = dev->_fnTable.vkGetImageMemoryRequirements2KHR;
        }
        allocatorInfo.pVulkanFunctions = &fn;

        vmaCreateAllocator(&allocatorInfo, &dev->_allocator);

        //dev->_isValid = true;

        //Fill driver and api info
        //if (dev->_features.supportsDrvPropQuery)
        //{
            //VkPhysicalDeviceDriverProperties driverProps{};
            //driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            //driverProps.pNext = nullptr;
//
            //VkPhysicalDeviceProperties2KHR deviceProps{};
            //deviceProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            //deviceProps.pNext = &driverProps;
//
            //vkGetPhysicalDeviceProperties2(dev->_phyDev.handle, &deviceProps);
//
            //VkConformanceVersion conforming = driverProps.conformanceVersion;
            //dev->_apiVer.major = conforming.major;
            //dev->_apiVer.minor = conforming.minor;
            //dev->_apiVer.subminor = conforming.subminor;
            //dev->_apiVer.patch = conforming.major;
            //dev->_drvName = driverProps.driverName;
            //dev->_drvInfo = driverProps.driverInfo;
        //}


        VkPhysicalDeviceProperties deviceProps{};
        vkGetPhysicalDeviceProperties(adp->GetHandle(), &deviceProps);

        dev->_commonFeat.computeShader = dev->_features.hasComputeCap;
        dev->_commonFeat.geometryShader = deviceFeatures.geometryShader;
        dev->_commonFeat.tessellationShaders = deviceFeatures.tessellationShader;
        dev->_commonFeat.multipleViewports = deviceFeatures.multiViewport;
        dev->_commonFeat.samplerLodBias = true;
        dev->_commonFeat.drawBaseVertex = true;
        dev->_commonFeat.drawBaseInstance = true;
        dev->_commonFeat.drawIndirect = true;
        dev->_commonFeat.drawIndirectBaseInstance = deviceFeatures.drawIndirectFirstInstance;
        dev->_commonFeat.fillModeWireframe = deviceFeatures.fillModeNonSolid;
        dev->_commonFeat.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
        dev->_commonFeat.depthClipDisable = deviceFeatures.depthClamp;
        dev->_commonFeat.texture1D = true;
        dev->_commonFeat.independentBlend = deviceFeatures.independentBlend;
        dev->_commonFeat.structuredBuffer = true;
        dev->_commonFeat.subsetTextureView = true;
        dev->_commonFeat.commandListDebugMarkers = dev->_features.supportsDebug;
        dev->_commonFeat.bufferRangeBinding = true;
        dev->_commonFeat.shaderFloat64 = deviceFeatures.shaderFloat64;

        return dev;
	}
    ICommandQueue* VulkanDevice::GetGfxCommandQueue() {
        return _gfxQ;
    }
    ICommandQueue* VulkanDevice::GetCopyCommandQueue() {
        return _copyQ;
    }

    
    IPhysicalAdapter& VulkanDevice::GetAdapter() const {
        return *_adp;
    }

    //void VulkanDevice::SubmitCommand(const CommandList* cmd ){
//
    //    assert(cmd != nullptr);
    //    auto* vkCmd = PtrCast<VulkanCommandList>(cmd);
    //    auto rawCmdBuf = vkCmd->GetHandle();
    //        
    //    VkSubmitInfo info {};
    //    info.commandBufferCount = 1;
    //    info.pCommandBuffers = &rawCmdBuf;
    //   
    //    VK_CHECK(vkQueueSubmit(
    //        _queueGraphics, 1, &info, nullptr
    //    ));
    //}

    ISwapChain::State VulkanDevice::PresentToSwapChain(
        ISwapChain* sc
    ) {
        
        VulkanSwapChain* vkSC = PtrCast<VulkanSwapChain>(sc);
        auto tex = vkSC->GetCurrentColorTarget()->GetTextureObject().get();
        
        auto sem = _gfxQ->PrepareTextureForPresent(PtrCast<VulkanTexture>(tex));

        VkSwapchainKHR deviceSwapchain = vkSC->GetHandle();
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &sem;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &deviceSwapchain;
        auto imageIndex = vkSC->GetCurrentImageIdx();
        presentInfo.pImageIndices = &imageIndex;
        vkSC->MarkCurrentImageInUse();


        //object presentLock = vkSC.PresentQueueIndex == _graphicsQueueIndex ? _graphicsQueueLock : vkSC;
        //lock(presentLock)
        {
            auto res = _fnTable.vkQueuePresentKHR(_gfxQ->GetHandle(), &presentInfo);
            //if (vkSC.AcquireNextImage(_device, VkSemaphore.Null, vkSC.ImageAvailableFence))
            //{
            //    Vulkan.VkFence fence = vkSC.ImageAvailableFence;
            //    vkWaitForFences(_device, 1, ref fence, true, ulong.MaxValue);
            //    vkResetFences(_device, 1, ref fence);
            //}
            switch (res)
            {
            case VK_SUCCESS:
                return ISwapChain::State::Optimal;
            case VK_SUBOPTIMAL_KHR:
                return ISwapChain::State::Suboptimal;
            case VK_ERROR_OUT_OF_DATE_KHR:
                return ISwapChain::State::OutOfDate;
            default:
                return ISwapChain::State::Error;
            }
        }
    }

    //bool VulkanDevice::WaitForFences(const sp<Fence>& fence, std::uint32_t timeOutNs) {
    //    auto vkFence = PtrCast<VulkanFence>(fence.get());
    //
    //    VkResult result = vkWaitForFences(_dev, 1, &vkFence->GetHandle(), true, timeOutNs);
    //    return result == VkResult::VK_SUCCESS;
    //}

    //sp<_CmdPoolContainer> VulkanDevice::GetCmdPool() { return _cmdPoolMgr.GetOnePool(); }
    _DescriptorSet VulkanDevice::AllocateDescriptorSet(VkDescriptorSetLayout layout){
        return _descPoolMgr.Allocate(layout);
    }

    common::sp<IBuffer> VulkanBuffer::Make(
        const common::sp<VulkanDevice>& dev,
        const IBuffer::Description& desc
    ) {
        
        auto& usage = desc.usage;

        VkBufferUsageFlags usages = VK_BUFFER_USAGE_TRANSFER_SRC_BIT 
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if ((desc.usage.vertexBuffer))
        {
            usages |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        if ((desc.usage.indexBuffer))
        {
            usages |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        if ((desc.usage.uniformBuffer))
        {
            usages |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if ((desc.usage.structuredBufferReadWrite)
            || (desc.usage.structuredBufferReadOnly))
        {
            usages |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if ((desc.usage.indirectBuffer))
        {
            usages |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }

        //bool isStaging = desc.usage.staging;
        //bool hostVisible = isStaging || desc.usage.dynamic;

        

        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = desc.sizeInBytes;
        bufferInfo.usage = usages;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        switch (desc.hostAccess)
        {        
        case HostAccess::PreferRead:
            //allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
        case HostAccess::PreferWrite:
            //allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        case HostAccess::None:
            allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        default:
            break;
        }

        /*if(hostVisible){
            allocInfo.requiredFlags |= 
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }else{
            allocInfo.requiredFlags |= 
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        if(isStaging){
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            // Use "host cached" memory for staging when available, 
            // for better performance of GPU -> CPU transfers
            allocInfo.preferredFlags |= 
                VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        }*/

        VkBuffer buffer;
        VmaAllocation allocation;
        auto res = vmaCreateBuffer(dev->Allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

        if(res != VK_SUCCESS) return nullptr;

        auto buf = new VulkanBuffer{ dev, desc };
        buf->_buffer = buffer;
        //buf->_size = size;
        buf->_allocation = allocation;

        //buf->_usages = usages;
        //buf->_allocationType = allocationType;

        alloy::utils::BufferState state {};
        state.access = ResourceAccess::None;
        state.stage = PipelineStage::None;
        dev->RegisterBufferState(state, buf);
		buf->RegisterTimeline(dev.get());

        return common::sp(buf);
    }

    VulkanBuffer::~VulkanBuffer(){
        vmaDestroyBuffer(_dev->Allocator(), _buffer, _allocation);

        DEBUGCODE(_dev = nullptr);
        DEBUGCODE(_buffer = VK_NULL_HANDLE);
        DEBUGCODE(_allocation = VK_NULL_HANDLE);
    }

    void* VulkanBuffer::MapToCPU()
    {
        void* mappedData;
        auto res = vmaMapMemory(_dev->Allocator(), _allocation, &mappedData);
        if (res != VK_SUCCESS) return nullptr;
        return mappedData;
    }

    void VulkanBuffer::UnMap()
    {
        vmaUnmapMemory(_dev->Allocator(), _allocation);
    }

    VulkanFence::~VulkanFence()
    {
        _dev->GetFnTable().vkDestroySemaphore(_dev->LogicalDev(), _timelineSem, nullptr);
    }
    
    uint64_t VulkanFence::GetSignaledValue() {
        uint64_t value;
        _dev->GetFnTable().vkGetSemaphoreCounterValueKHR(_dev->LogicalDev(), _timelineSem, &value);

        return value;
    }
    void VulkanFence::SignalFromCPU(uint64_t signalValue) {
        RegisterSyncPoint(_dev.get(), signalValue);
        VkSemaphoreSignalInfo signalInfo {};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signalInfo.pNext = nullptr;
        signalInfo.semaphore = _timelineSem;
        signalInfo.value = signalValue;
        VK_DEV_CALL(_dev, vkSignalSemaphoreKHR(_dev->LogicalDev(), &signalInfo));
    }
    bool VulkanFence::WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) {

        VkSemaphoreWaitInfo waitInfo {};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.pNext = nullptr;
        waitInfo.flags = 0;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &_timelineSem;
        waitInfo.pValues = &expectedValue;

        auto res = VK_DEV_CALL(_dev, vkWaitSemaphoresKHR(_dev->LogicalDev(), &waitInfo, timeoutMs * 1000000));

        switch (res) {
        //On success, this command returns
            case VK_SUCCESS: {
                SyncTimelineToThis(_dev.get(), expectedValue);
                return true;
            }
            case VK_TIMEOUT: return false;

        //On failure, this command returns
            case VK_ERROR_OUT_OF_HOST_MEMORY:
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            case VK_ERROR_DEVICE_LOST:
            default:
                //throw std::exception("wait returns error: %d");
                break;
        }

        return false;
    }

    common::sp<IEvent> VulkanFence::Make(const common::sp<VulkanDevice>& dev)
    {
        VkSemaphoreTypeCreateInfo timelineCreateInfo {};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.pNext = NULL;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0;

        VkSemaphoreCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &timelineCreateInfo;
        createInfo.flags = 0;

        VkSemaphore timelineSemaphore;
        VK_CHECK(VK_DEV_CALL(dev,
            vkCreateSemaphore(dev->LogicalDev(), &createInfo, nullptr, &timelineSemaphore)));

        auto fen = new VulkanFence(dev);
        fen->_timelineSem = timelineSemaphore;

        return common::sp<IEvent>(fen);
    }

    VkCommandBuffer _CmdPoolContainer::AllocateBuffer(){
        assert(std::this_thread::get_id() == boundID);

        VkCommandBufferAllocateInfo cbufInfo{};
        cbufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbufInfo.commandPool = pool;
        cbufInfo.commandBufferCount = 1;
        cbufInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VkCommandBuffer cbuf;
        VK_CHECK(VK_DEV_CALL(mgr->_dev, 
            vkAllocateCommandBuffers(mgr->_dev->LogicalDev(), &cbufInfo, &cbuf)));
        return cbuf;

    }

    void _CmdPoolContainer::FreeBuffer(VkCommandBuffer buf) {
        VK_DEV_CALL(mgr->_dev,
            vkFreeCommandBuffers(mgr->_dev->LogicalDev(), pool, 1, &buf));
    }
    
    void _CmdPoolMgr::_ReleaseCmdPoolHolder(_CmdPoolContainer* holder) {
        std::scoped_lock _lock{ _m_cmdPool };
        VK_DEV_CALL(_dev, vkResetCommandPool(_dev->LogicalDev(), holder->pool, 0));
        _threadBoundCmdPools.erase(holder->boundID);
        _freeCmdPools.push_back(holder->pool);
    }

    common::sp<_CmdPoolContainer> _CmdPoolMgr::_AcquireCmdPoolHolder() {
        _CmdPoolContainer* holder = nullptr;
        std::scoped_lock _lock{ _m_cmdPool };
        auto id = std::this_thread::get_id();
        auto res = _threadBoundCmdPools.find(id);
        if (res != _threadBoundCmdPools.end()) {
            holder = (*res).second;
            holder->ref();
        }
        else {
            VkCommandPool raw_pool;
            //try to acquire a free command pool
            if (!_freeCmdPools.empty()) { raw_pool = _freeCmdPools.front(); _freeCmdPools.pop_front(); }
            else {
                //Create a new command pool
                VkCommandPoolCreateInfo cmdPoolCI{};
                cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                cmdPoolCI.flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                                | VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                cmdPoolCI.queueFamilyIndex = _queueFamily;

                VK_CHECK(VK_DEV_CALL(_dev,
                    vkCreateCommandPool(_dev->LogicalDev(), &cmdPoolCI, nullptr, &raw_pool)));
            }

            holder = new _CmdPoolContainer{};
            holder->mgr = this;
            holder->pool = raw_pool;
            holder->boundID = id;

            //Add holder to the record
            _threadBoundCmdPools.emplace(id, holder);
        }

        return common::sp(holder);
    }

    _CmdPoolMgr::~_CmdPoolMgr() {
        //Threoretically there should be no bound command pools,
        // i.e. all command buffers holded by threads should be released
        // then the VulkanDevice can be destroyed.
        assert(_threadBoundCmdPools.empty());
        for (auto p : _freeCmdPools) {
            VK_DEV_CALL(_dev, vkDestroyCommandPool(_dev->LogicalDev(), p, nullptr));
        }
    }

    VulkanCommandQueue::VulkanCommandQueue(
        VulkanDevice* dev,
        std::uint32_t queueFamily,
        VkQueue q
    )
        : _dev(dev)
        , _cmdPoolMgr(dev, queueFamily)
        , _q(q)
        , _lastSubmittedFence(0)
        , ITrackerCmdQ(*dev)
    { 
        
        VkSemaphoreTypeCreateInfo timelineCreateInfo {};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.pNext = NULL;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0;

        VkSemaphoreCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &timelineCreateInfo;
        createInfo.flags = 0;

        VK_CHECK(VK_DEV_CALL(dev,
            vkCreateSemaphore(dev->LogicalDev(), &createInfo, nullptr, &_submissionFence)));

        createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        
        VK_CHECK(VK_DEV_CALL(dev,
            vkCreateSemaphore(dev->LogicalDev(), &createInfo, nullptr, &_presentFence)));
    }

    VulkanCommandQueue::~VulkanCommandQueue() {
        VK_DEV_CALL(_dev,vkDestroySemaphore(_dev->LogicalDev(), _submissionFence, nullptr));
        VK_DEV_CALL(_dev,vkDestroySemaphore(_dev->LogicalDev(), _presentFence, nullptr));
    }

    void VulkanCommandQueue::EncodeSignalEvent(IEvent* event, uint64_t value) {
        
        super::EncodeSignalEvent(event, value);

        auto rawFence = PtrCast<VulkanFence>(event)->GetHandle();
        const uint64_t signalValue = value; // Set semaphore value

        VkTimelineSemaphoreSubmitInfo timelineInfo {};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = NULL;
        timelineInfo.signalSemaphoreValueCount = 1;
        timelineInfo.pSignalSemaphoreValues = &signalValue;

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.signalSemaphoreCount  = 1;
        submitInfo.pSignalSemaphores = &rawFence;

        VK_DEV_CALL(_dev, vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE));

    }

    void VulkanCommandQueue::EncodeWaitForEvent(IEvent* evt, uint64_t value) {
        super::EncodeWaitForEvent(evt, value);
        auto rawFence = PtrCast<VulkanFence>(evt)->GetHandle();
        const uint64_t waitValue = value; // Wait until semaphore value is >= 2

        VkTimelineSemaphoreSubmitInfo timelineInfo {};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = NULL;
        timelineInfo.waitSemaphoreValueCount = 1;
        timelineInfo.pWaitSemaphoreValues = &waitValue;

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &rawFence;

        VK_DEV_CALL(_dev, vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE));
    }

    
    void VulkanCommandQueue::_RecycleTransitionCmdBufs() {
        uint64_t value;
        _dev->GetFnTable().vkGetSemaphoreCounterValueKHR(
            _dev->LogicalDev(), _submissionFence, &value);

        while(! _transitionCmdBufs.empty()) {
            auto& container = _transitionCmdBufs.front();
            if(container.completionFenceValue <= value) {
                container.cmdPool->FreeBuffer(container.cmdBuf);
                _transitionCmdBufs.pop_front();
            }
        }

    }
    
    void VulkanCommandQueue::InsertBarriers(
        const alloy::utils::BarrierActions& barriers
    ) {
        //Recycle completed transition command buffers
        _RecycleTransitionCmdBufs();

        TransitionCmdBuf container {};
        
        auto cmdPool = _cmdPoolMgr.GetOnePool();
        auto cmdBuf = cmdPool->AllocateBuffer();
        container.cmdBuf = cmdBuf;
        container.cmdPool = std::move(cmdPool);
        container.completionFenceValue = _lastSubmittedFence;

        {
            VkCommandBufferBeginInfo beginInfo {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.pNext = NULL;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = NULL;
            VK_DEV_CALL(_dev, vkBeginCommandBuffer(cmdBuf, &beginInfo));
            InsertBarrier(_dev, cmdBuf, barriers);
            VK_DEV_CALL(_dev, vkEndCommandBuffer(cmdBuf));
        }

        _transitionCmdBufs.push_back(container);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuf;

        VK_DEV_CALL(_dev, vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE));
    }

    
    VkSemaphore VulkanCommandQueue::PrepareTextureForPresent(VulkanTexture* tex) {
        alloy::utils::TextureState currState {};
        auto it = _currentState.textures.find(tex);
        if(it != _currentState.textures.end()) {
            currState = it->second;
        } else {
            currState = _cpuTimeline.GetCurrentState().textures[tex];
            _currentState.textures[tex] = currState;
        }

        if(currState.layout != alloy::TextureLayout::PRESENT) {
            auto& texDesc = tex->GetDesc();
            VkImageMemoryBarrier barrier {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = 0;//vk_access_flags_from_d3d12_barrier(currState.access);
            barrier.dstAccessMask = 0;
            barrier.oldLayout = AlToVkTexLayout(currState.layout);
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = tex->GetHandle();
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, texDesc.mipLevels, 0, texDesc.arrayLayers };

            auto stagesBefore = VK_PIPELINE_STAGE_NONE_KHR;//vk_stage_flags_from_d3d12_barrier(currState.stage, currState.access);
            

            _RecycleTransitionCmdBufs();

            TransitionCmdBuf container {};
            
            _lastSubmittedFence++;
            auto cmdPool = _cmdPoolMgr.GetOnePool();
            auto cmdBuf = cmdPool->AllocateBuffer();
            container.cmdBuf = cmdBuf;
            container.cmdPool = std::move(cmdPool);
            container.completionFenceValue = _lastSubmittedFence;

            {
                VkCommandBufferBeginInfo beginInfo {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.pNext = NULL;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                beginInfo.pInheritanceInfo = NULL;
                VK_DEV_CALL(_dev, vkBeginCommandBuffer(cmdBuf, &beginInfo));
                VK_DEV_CALL(_dev, vkCmdPipelineBarrier(
                    cmdBuf,
                    stagesBefore,
                    0,
                    0 /*VkDependencyFlags*/,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier));
                VK_DEV_CALL(_dev, vkEndCommandBuffer(cmdBuf));
            }

            _transitionCmdBufs.push_back(container);

            uint64_t signalValues[] = {_lastSubmittedFence, 0};

            VkTimelineSemaphoreSubmitInfo timelineInfo {};
            timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timelineInfo.pNext = NULL;
            timelineInfo.signalSemaphoreValueCount = 2;
            timelineInfo.pSignalSemaphoreValues = signalValues;

            VkSubmitInfo submitInfo {};
            VkSemaphore sem[] = {_submissionFence, _presentFence};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext = &timelineInfo;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuf;
            submitInfo.signalSemaphoreCount = 2;
            submitInfo.pSignalSemaphores = sem;


            VK_DEV_CALL(_dev, vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE));
        } else {
            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &_presentFence;

            VK_DEV_CALL(_dev, vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE));
        }
    
        _currentState.textures[tex].stage = PipelineStage::None;
        _currentState.textures[tex].access = ResourceAccess::None;
        _currentState.textures[tex].layout = TextureLayout::PRESENT;

        return _presentFence;
    }

    void VulkanCommandQueue::SubmitCommand(ICommandList* cmd) {
        _lastSubmittedFence++;
        super::SubmitCommand(cmd);
        assert(cmd != nullptr);
        auto* vkCmd = PtrCast<VulkanCommandList>(cmd);
        auto rawCmdBuf = vkCmd->GetHandle();

        VkTimelineSemaphoreSubmitInfo timelineInfo {};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = NULL;
        timelineInfo.signalSemaphoreValueCount = 1;
        timelineInfo.pSignalSemaphoreValues = &_lastSubmittedFence;
            
        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &rawCmdBuf;
        submitInfo.signalSemaphoreCount  = 1;
        submitInfo.pSignalSemaphores = &_submissionFence;
       
        VK_CHECK(VK_DEV_CALL(_dev, vkQueueSubmit(
            _q, 1, &submitInfo, nullptr
        )));

        super::NotifySyncResources();
    }

    common::sp<ICommandList> VulkanCommandQueue::CreateCommandList(){

        auto cmdPool = _cmdPoolMgr.GetOnePool();
        auto vkCmdBuf = cmdPool->AllocateBuffer();

        _dev->ref();
        auto cmdBuf = new VulkanCommandList(common::sp(_dev), vkCmdBuf, std::move(cmdPool));

        return common::sp<ICommandList>(cmdBuf);
    
    }

    //sp<_CmdPoolContainer> _CmdPoolMgr::GetOnePool() { return _AcquireCmdPoolHolder(); }
}

