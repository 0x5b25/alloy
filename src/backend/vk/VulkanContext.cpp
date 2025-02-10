#include "VulkanContext.hpp"

#include "VulkanDevice.hpp"

namespace alloy
{
    
    common::sp<IContext> CreateVulkanContext() {
        return alloy::vk::VulkanContext::Make();
    }

} // namespace alloy


namespace alloy::vk
{
    uint32_t VulkanContext::VolkCtx::_refCnt = 0;
    std::mutex VulkanContext::VolkCtx::_m;
    VulkanContext::Features VulkanContext::VolkCtx::_features {};
    VkDebugReportCallbackEXT VulkanContext::VolkCtx::_debugCallbackHandle = VK_NULL_HANDLE;


    
    common::sp<IGraphicsDevice> VulkanContext::CreateDefaultDevice(
        const IGraphicsDevice::Options& options
    ) {
        auto adps = EnumerateAdapters();
        if(adps.empty()) {
            return nullptr;
        }

        return adps.front()->RequestDevice(options);
    }


    
    common::sp<VulkanAdapter> VulkanAdapter::Make(
        const common::sp<VulkanContext>& ctx,
        VkPhysicalDevice handle
    ) {

        //Find graphics queue
        std::uint32_t queueFamCnt;
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueFamCnt, nullptr);
        std::vector<VkQueueFamilyProperties> queue_family_properties(queueFamCnt);
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueFamCnt, queue_family_properties.data());

        if (queue_family_properties.empty())
        {
            return nullptr;
        }

        auto _FindUniqueFamilyWithFlags = [&](uint32_t flags, const std::vector<uint32_t>& excludes) {
            for (uint32_t j = 0; j < queue_family_properties.size(); j++)
            {
                bool shouldExclude = false;
                for (auto& f : excludes) if (f == j) { shouldExclude = true; break; }

                if (shouldExclude) continue;

                if (queue_family_properties[j].queueFlags & flags)return std::optional<uint32_t>(j);
            }
            return std::optional<uint32_t>();
        };

        
        uint32_t graphicsQueueFamily;
        std::vector<uint32_t> excludes;

        //Lets find a graphics queue first
        {
            while (true)
            {
                //uint32_t gQueue;
                auto res = _FindUniqueFamilyWithFlags(
                    VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT,
                    excludes);
                if (!res.has_value()) {
                    //No Graphics queue means no suitable device
                    return nullptr;
                }
                //Add to exclude list
                excludes.push_back(res.value());

                ////Check for presentation support
                //if (surface != VK_NULL_HANDLE) {
                //    VkBool32 supports_present;
                //    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
                //        phyDev, res.value(), surface, &supports_present)
                //    );
                //    if (!supports_present) {
                //        break;
                //    }
                //}
                // Find a queue family which supports graphics and presentation.
                //if (supports_present)
                {
                    graphicsQueueFamily = res.value();
                    break;
                }
            }
        }

        auto adp = new VulkanAdapter(ctx, handle);
        adp->_qFamily.graphicsQueueFamily = graphicsQueueFamily;

        VkPhysicalDeviceProperties devProp{};
        vkGetPhysicalDeviceProperties(handle, &devProp);
        adp->info.deviceName = devProp.deviceName;

        adp->info.apiVersion = {Backend::Vulkan, VK_API_VERSION_MAJOR(devProp.apiVersion)
                                               , VK_API_VERSION_MINOR(devProp.apiVersion)
                                               , 0
                                               , VK_API_VERSION_PATCH(devProp.apiVersion)};

        adp->info.driverVersion = devProp.driverVersion;
        adp->info.vendorID = devProp.vendorID;
        adp->info.deviceID = devProp.deviceID;

        adp->PopulateAdpMemSegs();
        adp->PopulateAdpLimits();

        //Score the device
        {
            auto& gQueueProp = queue_family_properties[adp->_qFamily.graphicsQueueFamily];
            if (gQueueProp.queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT) {
                adp->_qFamily.graphicsQueueSupportsCompute = true;
            }
        }
        
        if (devProp.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            ///#TODO: Default all integrated GPUs to UMA 
            adp->info.capabilities.isUMA = true;
        } else {
            ////Find if resizable bar is enabled
            //uint64_t hostVisibleVramSize = 0;
            //for(auto& seg : adp->info.memSegments) {
            //    if(seg.flags.isCPUVisible && !seg.flags.isInSysMem) {
            //        hostVisibleVramSize += seg.sizeInBytes;
            //    }
            //}
            //
            //if(hostVisibleVramSize > 256 * 1024 * 1024) {
            //    adp->info.capabilities.supportReBar = 1;
            //}
        }

        {
            //Try find dedicated compute queue
            auto res = _FindUniqueFamilyWithFlags(VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT, excludes);
            if (res.has_value()) {
                excludes.push_back(res.value());
                adp->_qFamily.computeQueueFamily = res.value();
            }
        }

        {
            //Try find dedicated transfer queue
            auto res = _FindUniqueFamilyWithFlags(VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT, excludes);
            if (res.has_value()) {
                excludes.push_back(res.value());
                adp->_qFamily.transferQueueFamily = res.value();
            }
        }
        

        return common::sp(adp);

    }
    
    void VulkanAdapter::PopulateAdpLimits() {
        
        VkPhysicalDeviceProperties devProp{};
        vkGetPhysicalDeviceProperties(_handle, &devProp);

        auto _VkSampleCnts2AlSampleCnt = [](VkSampleCountFlags flags) {
            if(flags & VK_SAMPLE_COUNT_64_BIT) return SampleCount::x32;
            if(flags & VK_SAMPLE_COUNT_32_BIT) return SampleCount::x32;
            if(flags & VK_SAMPLE_COUNT_16_BIT) return SampleCount::x16;
            if(flags & VK_SAMPLE_COUNT_8_BIT) return SampleCount::x8;
            if(flags & VK_SAMPLE_COUNT_4_BIT) return SampleCount::x4;
            if(flags & VK_SAMPLE_COUNT_2_BIT) return SampleCount::x2;
            return  SampleCount::x1;
        };

        info.limits.maxImageDimension1D = devProp.limits.maxImageDimension1D; 
        info.limits.maxImageDimension2D = devProp.limits.maxImageDimension2D; 
        info.limits.maxImageDimension3D = devProp.limits.maxImageDimension3D; 
        info.limits.maxImageDimensionCube = devProp.limits.maxImageDimensionCube; 
        info.limits.maxImageArrayLayers = devProp.limits.maxImageArrayLayers; 
        info.limits.maxTexelBufferElements = devProp.limits.maxTexelBufferElements; 
        info.limits.maxUniformBufferRange = devProp.limits.maxUniformBufferRange; 
        info.limits.maxStorageBufferRange = devProp.limits.maxStorageBufferRange; 
        info.limits.maxPushConstantsSize = devProp.limits.maxPushConstantsSize; 
        info.limits.maxMemoryAllocationCount = devProp.limits.maxMemoryAllocationCount; 
        info.limits.maxSamplerAllocationCount = devProp.limits.maxSamplerAllocationCount; 
        info.limits.bufferImageGranularity = devProp.limits.bufferImageGranularity; 
        info.limits.sparseAddressSpaceSize = devProp.limits.sparseAddressSpaceSize; 
        info.limits.maxBoundDescriptorSets = devProp.limits.maxBoundDescriptorSets; 
        info.limits.maxPerStageDescriptorSamplers = devProp.limits.maxPerStageDescriptorSamplers; 
        info.limits.maxPerStageDescriptorUniformBuffers = devProp.limits.maxPerStageDescriptorUniformBuffers; 
        info.limits.maxPerStageDescriptorStorageBuffers = devProp.limits.maxPerStageDescriptorStorageBuffers; 
        info.limits.maxPerStageDescriptorSampledImages = devProp.limits.maxPerStageDescriptorSampledImages; 
        info.limits.maxPerStageDescriptorStorageImages = devProp.limits.maxPerStageDescriptorStorageImages; 
        info.limits.maxPerStageDescriptorInputAttachments = devProp.limits.maxPerStageDescriptorInputAttachments; 
        info.limits.maxPerStageResources = devProp.limits.maxPerStageResources; 
        info.limits.maxVertexInputAttributes = devProp.limits.maxVertexInputAttributes; 
        info.limits.maxVertexInputBindings = devProp.limits.maxVertexInputBindings; 
        info.limits.maxVertexInputAttributeOffset = devProp.limits.maxVertexInputAttributeOffset; 
        info.limits.maxVertexInputBindingStride = devProp.limits.maxVertexInputBindingStride; 
        info.limits.maxVertexOutputComponents = devProp.limits.maxVertexOutputComponents; 
        info.limits.maxFragmentInputComponents = devProp.limits.maxFragmentInputComponents; 
        info.limits.maxFragmentOutputAttachments = devProp.limits.maxFragmentOutputAttachments; 
        info.limits.maxFragmentDualSrcAttachments = devProp.limits.maxFragmentDualSrcAttachments; 
        info.limits.maxFragmentCombinedOutputResources = devProp.limits.maxFragmentCombinedOutputResources; 
        info.limits.maxComputeSharedMemorySize = devProp.limits.maxComputeSharedMemorySize; 
        info.limits.maxComputeWorkGroupCount[0] = devProp.limits.maxComputeWorkGroupCount[0]; 
        info.limits.maxComputeWorkGroupCount[1] = devProp.limits.maxComputeWorkGroupCount[1]; 
        info.limits.maxComputeWorkGroupCount[2] = devProp.limits.maxComputeWorkGroupCount[2]; 
        info.limits.maxComputeWorkGroupInvocations = devProp.limits.maxComputeWorkGroupInvocations; 
        info.limits.maxComputeWorkGroupSize[0] = devProp.limits.maxComputeWorkGroupSize[0]; 
        info.limits.maxComputeWorkGroupSize[1] = devProp.limits.maxComputeWorkGroupSize[1]; 
        info.limits.maxComputeWorkGroupSize[2] = devProp.limits.maxComputeWorkGroupSize[2]; 
        info.limits.maxDrawIndexedIndexValue = devProp.limits.maxDrawIndexedIndexValue; 
        info.limits.maxDrawIndirectCount = devProp.limits.maxDrawIndirectCount; 
        info.limits.maxSamplerLodBias = devProp.limits.maxSamplerLodBias; 
        info.limits.maxSamplerAnisotropy = devProp.limits.maxSamplerAnisotropy; 
        info.limits.maxViewports = devProp.limits.maxViewports; 
        info.limits.maxViewportDimensions[0] = devProp.limits.maxViewportDimensions[0];
        info.limits.maxViewportDimensions[1] = devProp.limits.maxViewportDimensions[1];
        info.limits.minMemoryMapAlignment = devProp.limits.minMemoryMapAlignment; 
        info.limits.minTexelBufferOffsetAlignment = devProp.limits.minTexelBufferOffsetAlignment; 
        info.limits.minUniformBufferOffsetAlignment = devProp.limits.minUniformBufferOffsetAlignment; 
        info.limits.minStorageBufferOffsetAlignment = devProp.limits.minStorageBufferOffsetAlignment; 
        info.limits.maxFramebufferWidth = devProp.limits.maxFramebufferWidth; 
        info.limits.maxFramebufferHeight = devProp.limits.maxFramebufferHeight; 
        info.limits.maxFramebufferLayers = devProp.limits.maxFramebufferLayers; 
        info.limits.framebufferColorSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.framebufferColorSampleCounts); 
        info.limits.framebufferDepthSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.framebufferDepthSampleCounts); 
        info.limits.framebufferStencilSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.framebufferStencilSampleCounts); 
        info.limits.framebufferNoAttachmentsSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.framebufferNoAttachmentsSampleCounts); 
        info.limits.maxColorAttachments = devProp.limits.maxColorAttachments; 
        info.limits.sampledImageColorSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.sampledImageColorSampleCounts); 
        info.limits.sampledImageIntegerSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.sampledImageIntegerSampleCounts); 
        info.limits.sampledImageDepthSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.sampledImageDepthSampleCounts); 
        info.limits.sampledImageStencilSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.sampledImageStencilSampleCounts); 
        info.limits.storageImageSampleCounts 
            = _VkSampleCnts2AlSampleCnt(devProp.limits.storageImageSampleCounts); 
        info.limits.maxSampleMaskWords = devProp.limits.maxSampleMaskWords; 
        info.limits.timestampPeriod = devProp.limits.timestampPeriod; 
        info.limits.maxClipDistances = devProp.limits.maxClipDistances; 
        info.limits.maxCullDistances = devProp.limits.maxCullDistances; 
        info.limits.maxCombinedClipAndCullDistances = devProp.limits.maxCombinedClipAndCullDistances; 
        info.limits.pointSizeRange[0] = devProp.limits.pointSizeRange[0]; 
        info.limits.pointSizeRange[1] = devProp.limits.pointSizeRange[1]; 
        info.limits.lineWidthRange[0] = devProp.limits.lineWidthRange[0]; 
        info.limits.lineWidthRange[1] = devProp.limits.lineWidthRange[1]; 
        info.limits.pointSizeGranularity = devProp.limits.pointSizeGranularity; 
        info.limits.lineWidthGranularity = devProp.limits.lineWidthGranularity; 
    }

    void VulkanAdapter::PopulateAdpMemSegs() {
        info.memSegments.clear();
        //Evaluate vram size
        VkPhysicalDeviceMemoryProperties vramProp{};
        vkGetPhysicalDeviceMemoryProperties(_handle, &vramProp);

        auto _IsHeapSatisfied = [&](
            uint32_t heapIdx,
            VkMemoryPropertyFlags exist, 
            VkMemoryPropertyFlags nonExist
        ) {
            bool hasRequiredFlags = exist ? false : true;
            for (uint32_t i = 0; i < vramProp.memoryTypeCount; i++) {
                auto& type = vramProp.memoryTypes[i];
                if(type.heapIndex != heapIdx) continue;

                if(nonExist) {
                    if(type.propertyFlags & nonExist)
                        return false;
                }

                hasRequiredFlags |= (type.propertyFlags & exist) == exist;
            }
            return hasRequiredFlags;
        };

        //Find private vram segment
        {
            size_t segmemtSize = 0;
            for (uint32_t i = 0; i < vramProp.memoryHeapCount; i++) {
                auto& heap = vramProp.memoryHeaps[i];
                if( (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) )
                {
                    if(_IsHeapSatisfied(i, 0, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                        segmemtSize += heap.size;
                    }
                }
            }

            if(segmemtSize) {
                auto& seg = info.memSegments.emplace_back();
                seg.sizeInBytes = segmemtSize;
                seg.flags.isCPUVisible = false;
                seg.flags.isInSysMem = false;
            }
        }

        //Find host visible vram segment
        {
            size_t segmemtSize = 0;
            for (uint32_t i = 0; i < vramProp.memoryHeapCount; i++) {
                auto& heap = vramProp.memoryHeaps[i];
                if( (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) )
                {
                    if(_IsHeapSatisfied(i, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0)) {
                        segmemtSize += heap.size;
                    }
                }
            }

            if(segmemtSize) {
                auto& seg = info.memSegments.emplace_back();
                seg.sizeInBytes = segmemtSize;
                seg.flags.isCPUVisible = true;
                seg.flags.isInSysMem = false;
            }
        }

        //Find shared system memory
        {
            size_t segmemtSize = 0;
            for (uint32_t i = 0; i < vramProp.memoryHeapCount; i++) {
                auto& heap = vramProp.memoryHeaps[i];
                if( !(heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) )
                {
                    if(_IsHeapSatisfied(i, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0)) {
                        segmemtSize += heap.size;
                    }
                }
            }

            if(segmemtSize) {
                auto& seg = info.memSegments.emplace_back();
                seg.sizeInBytes = segmemtSize;
                seg.flags.isCPUVisible = true;
                seg.flags.isInSysMem = true;
            }
        }
    }

    VulkanAdapter::VulkanAdapter(
        const common::sp<VulkanContext>& ctx,
        VkPhysicalDevice handle
    ) : _ctx(ctx)
      , _handle(handle)
      , _qFamily{}
    { }

    
    common::sp<IGraphicsDevice> VulkanAdapter::RequestDevice(
        const IGraphicsDevice::Options& options
    ) {
        return VulkanDevice::Make(common::ref_sp(this), options);
    }

    std::vector<common::sp<IPhysicalAdapter>> VulkanContext::EnumerateAdapters() {

        const static std::vector<std::string> requiredExtensions = {
            VkDevExtNames::VK_KHR_TIMELINE_SEMAPHORE,
            VkDevExtNames::VK_KHR_DYNAMIC_RENDERING,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VkDevExtNames::VK_KHR_MAINTENANCE1,
        };

        auto _IsAdpSupportRequiredExts = [&](VkPhysicalDevice adp)->bool {
            //Does device supports required extensions
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(adp, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(adp, nullptr, &extensionCount, availableExtensions.data());

            std::unordered_set<std::string> reqExtSet(requiredExtensions.begin(), requiredExtensions.end());

            for (const auto& extension : availableExtensions) {
                reqExtSet.erase(extension.extensionName);
            }

            return reqExtSet.empty();
        };

        std::uint32_t devCnt;
        VK_CHECK(vkEnumeratePhysicalDevices(_instance, &devCnt, nullptr));
        std::vector<VkPhysicalDevice> gpus(devCnt);
        VK_CHECK(vkEnumeratePhysicalDevices(_instance, &devCnt, gpus.data()));

        std::vector<common::sp<IPhysicalAdapter>> adapters;
        
        for(auto& gpu : gpus) {

            if(!_IsAdpSupportRequiredExts(gpu)) continue;

            auto adp = VulkanAdapter::Make(common::ref_sp(this), gpu);
            if(adp) {
                adapters.push_back(adp);
            }
        };

        return adapters;
    }


} // namespace alloy::vk

