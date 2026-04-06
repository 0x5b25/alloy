#include "VulkanContext.hpp"

#include "VulkanDevice.hpp"
#include <unordered_set>
#include <string>

namespace alloy
{
    
    common::sp<IContext> CreateVulkanContext(const IContext::Options& opts) {
        return alloy::vk::VulkanContext::Make(opts);
    }

} // namespace alloy


namespace alloy::vk
{
    void VulkanDevCaps::ReadFromDevice(VulkanContext& ctx, VkPhysicalDevice adp) {
        *this = { };

        auto& fnTable = ctx.GetFnTable();
        
        VkPhysicalDeviceProperties devProps { };
        fnTable.vkGetPhysicalDeviceProperties(adp, &devProps);

        apiVersion = {Backend::Vulkan, VK_API_VERSION_MAJOR(devProps.apiVersion)
                                     , VK_API_VERSION_MINOR(devProps.apiVersion)
                                     , 0
                                     , VK_API_VERSION_PATCH(devProps.apiVersion)};

        limits = devProps.limits;
        sparseProperties = devProps.sparseProperties;

        isIntegratedGPU 
            = devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

        fnTable.vkGetPhysicalDeviceFeatures(adp, &features);

        if(devProps.apiVersion >= VK_VERSION_1_1) {

            VkPhysicalDeviceProperties2 devProps2 { 
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
            };

            VkPhysicalDeviceFeatures2 features2 { 
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
            };

            properties11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            devProps2.pNext = &properties11;

            features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features2.pNext = &features11;

            if(devProps.apiVersion >= VK_API_VERSION_1_2) {
                properties12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
                properties12.pNext = devProps2.pNext;
                devProps2.pNext = &properties12;

                features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                features12.pNext = features2.pNext;
                features2.pNext = &features12;
            }

            if(devProps.apiVersion >= VK_API_VERSION_1_3) {
                properties13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
                properties13.pNext = devProps2.pNext;
                devProps2.pNext = &properties13;

                features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                features13.pNext = features2.pNext;
                features2.pNext = &features13;
            }

            if(devProps.apiVersion >= VK_API_VERSION_1_4) {
                properties14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;
                properties14.pNext = devProps2.pNext;
                devProps2.pNext = &properties14;

                features14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
                features14.pNext = features2.pNext;
                features2.pNext = &features14;
            }

            fnTable.vkGetPhysicalDeviceProperties2(adp, &devProps2);
            fnTable.vkGetPhysicalDeviceFeatures2(adp, &features2);
        }

        //Resizable BAR system will have a >256MB HOST_VISIBLE DEVICE_LOCAL heap
        VkPhysicalDeviceMemoryProperties vramProp{};
        fnTable.vkGetPhysicalDeviceMemoryProperties(adp, &vramProp);
        constexpr auto BARPropBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        constexpr size_t defaultBARSize = 256 * 1024 * 1024; //256MB

        for(uint32_t i = 0; i < vramProp.memoryTypeCount; i++) {
            auto& type = vramProp.memoryTypes[i];

            auto isBARType = type.propertyFlags & BARPropBits == BARPropBits;
            if(isBARType) {
                auto& heap = vramProp.memoryHeaps[type.heapIndex];
                if(heap.size > defaultBARSize) {
                    isResizableBARSupported = true;
                    break;
                }
            }
        }

        //Get all supported extensions
        uint32_t extensionCount;
        fnTable.vkEnumerateDeviceExtensionProperties(adp, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        fnTable.vkEnumerateDeviceExtensionProperties(adp, nullptr, &extensionCount, availableExtensions.data());

        std::unordered_set<std::string> availableExtNames;
        for(auto& ext : availableExtensions) {
            availableExtNames.emplace(ext.extensionName);
        }

        auto IsExtSupported = [&](const std::string& extName) {
            return availableExtNames.contains(extName);
        };

        if( devProps.apiVersion >= VK_VERSION_1_4 || 
            IsExtSupported(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME)
        ) {
            features12.scalarBlockLayout = true;
        }

        if(IsExtSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME)) {
            supportMeshShader = true;
        }

        if( IsExtSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
            IsExtSupported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) &&
            IsExtSupported(VK_KHR_RAY_QUERY_EXTENSION_NAME)
        ) {
            supportRayTracing = true;
        }

        if(IsExtSupported(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
            supportBindless = true;
        }
    }




    uint32_t VulkanContext::VolkCtx::_refCnt = 0;
    std::mutex VulkanContext::VolkCtx::_m;

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
        auto& fnTable = ctx->GetFnTable();

        //Find graphics queue
        std::uint32_t queueFamCnt;
        fnTable.vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueFamCnt, nullptr);
        std::vector<VkQueueFamilyProperties> queue_family_properties(queueFamCnt);
        fnTable.vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueFamCnt, queue_family_properties.data());

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
        fnTable.vkGetPhysicalDeviceProperties(handle, &devProp);

        adp->info.deviceName = devProp.deviceName;
        adp->info.driverVersion = devProp.driverVersion;
        adp->info.vendorID = devProp.vendorID;
        adp->info.deviceID = devProp.deviceID;

        adp->PopulateAdpFeatures();
        adp->info.apiVersion = adp->_caps.apiVersion;
        adp->PopulateAdpMemSegs();
        adp->PopulateAdpLimits();

        //Score the device
        {
            auto& gQueueProp = queue_family_properties[adp->_qFamily.graphicsQueueFamily];
            if (gQueueProp.queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT) {
                adp->_qFamily.graphicsQueueSupportsCompute = true;
            }
        }
        
        if (adp->_caps.isIntegratedGPU) {
            ///#TODO: Default all integrated GPUs to UMA 
            adp->info.capabilities.isUMA = true;
        } 

        if(adp->_caps.isResizableBARSupported) {
            adp->info.capabilities.supportResizableBar = 1;
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
        auto& fnTable = _ctx->GetFnTable();

        auto _VkSampleCnts2AlSampleCnt = [](VkSampleCountFlags flags) {
            if(flags & VK_SAMPLE_COUNT_64_BIT) return SampleCount::x32;
            if(flags & VK_SAMPLE_COUNT_32_BIT) return SampleCount::x32;
            if(flags & VK_SAMPLE_COUNT_16_BIT) return SampleCount::x16;
            if(flags & VK_SAMPLE_COUNT_8_BIT) return SampleCount::x8;
            if(flags & VK_SAMPLE_COUNT_4_BIT) return SampleCount::x4;
            if(flags & VK_SAMPLE_COUNT_2_BIT) return SampleCount::x2;
            return  SampleCount::x1;
        };

        info.limits.maxImageDimension1D = _caps.limits.maxImageDimension1D; 
        info.limits.maxImageDimension2D = _caps.limits.maxImageDimension2D; 
        info.limits.maxImageDimension3D = _caps.limits.maxImageDimension3D; 
        info.limits.maxImageDimensionCube = _caps.limits.maxImageDimensionCube; 
        info.limits.maxImageArrayLayers = _caps.limits.maxImageArrayLayers; 
        info.limits.maxTexelBufferElements = _caps.limits.maxTexelBufferElements; 
        info.limits.maxUniformBufferRange = _caps.limits.maxUniformBufferRange; 
        info.limits.maxStorageBufferRange = _caps.limits.maxStorageBufferRange; 
        info.limits.maxPushConstantsSize = _caps.limits.maxPushConstantsSize; 
        info.limits.maxMemoryAllocationCount = _caps.limits.maxMemoryAllocationCount; 
        info.limits.maxSamplerAllocationCount = _caps.limits.maxSamplerAllocationCount; 
        info.limits.bufferImageGranularity = _caps.limits.bufferImageGranularity; 
        info.limits.sparseAddressSpaceSize = _caps.limits.sparseAddressSpaceSize; 
        info.limits.maxBoundDescriptorSets = _caps.limits.maxBoundDescriptorSets; 
        info.limits.maxPerStageDescriptorSamplers = _caps.limits.maxPerStageDescriptorSamplers; 
        info.limits.maxPerStageDescriptorUniformBuffers = _caps.limits.maxPerStageDescriptorUniformBuffers; 
        info.limits.maxPerStageDescriptorStorageBuffers = _caps.limits.maxPerStageDescriptorStorageBuffers; 
        info.limits.maxPerStageDescriptorSampledImages = _caps.limits.maxPerStageDescriptorSampledImages; 
        info.limits.maxPerStageDescriptorStorageImages = _caps.limits.maxPerStageDescriptorStorageImages; 
        info.limits.maxPerStageDescriptorInputAttachments = _caps.limits.maxPerStageDescriptorInputAttachments; 
        info.limits.maxPerStageResources = _caps.limits.maxPerStageResources; 
        info.limits.maxVertexInputAttributes = _caps.limits.maxVertexInputAttributes; 
        info.limits.maxVertexInputBindings = _caps.limits.maxVertexInputBindings; 
        info.limits.maxVertexInputAttributeOffset = _caps.limits.maxVertexInputAttributeOffset; 
        info.limits.maxVertexInputBindingStride = _caps.limits.maxVertexInputBindingStride; 
        info.limits.maxVertexOutputComponents = _caps.limits.maxVertexOutputComponents; 
        info.limits.maxFragmentInputComponents = _caps.limits.maxFragmentInputComponents; 
        info.limits.maxFragmentOutputAttachments = _caps.limits.maxFragmentOutputAttachments; 
        info.limits.maxFragmentDualSrcAttachments = _caps.limits.maxFragmentDualSrcAttachments; 
        info.limits.maxFragmentCombinedOutputResources = _caps.limits.maxFragmentCombinedOutputResources; 
        info.limits.maxComputeSharedMemorySize = _caps.limits.maxComputeSharedMemorySize; 
        info.limits.maxComputeWorkGroupCount[0] = _caps.limits.maxComputeWorkGroupCount[0]; 
        info.limits.maxComputeWorkGroupCount[1] = _caps.limits.maxComputeWorkGroupCount[1]; 
        info.limits.maxComputeWorkGroupCount[2] = _caps.limits.maxComputeWorkGroupCount[2]; 
        info.limits.maxComputeWorkGroupInvocations = _caps.limits.maxComputeWorkGroupInvocations; 
        info.limits.maxComputeWorkGroupSize[0] = _caps.limits.maxComputeWorkGroupSize[0]; 
        info.limits.maxComputeWorkGroupSize[1] = _caps.limits.maxComputeWorkGroupSize[1]; 
        info.limits.maxComputeWorkGroupSize[2] = _caps.limits.maxComputeWorkGroupSize[2]; 
        info.limits.maxDrawIndexedIndexValue = _caps.limits.maxDrawIndexedIndexValue; 
        info.limits.maxDrawIndirectCount = _caps.limits.maxDrawIndirectCount; 
        info.limits.maxSamplerLodBias = _caps.limits.maxSamplerLodBias; 
        info.limits.maxSamplerAnisotropy = _caps.limits.maxSamplerAnisotropy; 
        info.limits.maxViewports = _caps.limits.maxViewports; 
        info.limits.maxViewportDimensions[0] = _caps.limits.maxViewportDimensions[0];
        info.limits.maxViewportDimensions[1] = _caps.limits.maxViewportDimensions[1];
        info.limits.minMemoryMapAlignment = _caps.limits.minMemoryMapAlignment; 
        info.limits.minTexelBufferOffsetAlignment = _caps.limits.minTexelBufferOffsetAlignment; 
        info.limits.minUniformBufferOffsetAlignment = _caps.limits.minUniformBufferOffsetAlignment; 
        info.limits.minStorageBufferOffsetAlignment = _caps.limits.minStorageBufferOffsetAlignment; 
        info.limits.maxFramebufferWidth = _caps.limits.maxFramebufferWidth; 
        info.limits.maxFramebufferHeight = _caps.limits.maxFramebufferHeight; 
        info.limits.maxFramebufferLayers = _caps.limits.maxFramebufferLayers; 
        //info.limits.framebufferColorSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.framebufferColorSampleCounts); 
        //info.limits.framebufferDepthSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.framebufferDepthSampleCounts); 
        //info.limits.framebufferStencilSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.framebufferStencilSampleCounts); 
        //info.limits.framebufferNoAttachmentsSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.framebufferNoAttachmentsSampleCounts); 
        info.limits.maxColorAttachments = _caps.limits.maxColorAttachments; 
        //info.limits.sampledImageColorSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.sampledImageColorSampleCounts); 
        //info.limits.sampledImageIntegerSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.sampledImageIntegerSampleCounts); 
        //info.limits.sampledImageDepthSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.sampledImageDepthSampleCounts); 
        //info.limits.sampledImageStencilSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.sampledImageStencilSampleCounts); 
        //info.limits.storageImageSampleCounts 
        //    = _VkSampleCnts2AlSampleCnt(_caps.limits.storageImageSampleCounts); 
        //info.limits.maxSampleMaskWords = _caps.limits.maxSampleMaskWords; 
        info.limits.timestampPeriod = _caps.limits.timestampPeriod; 
        info.limits.maxClipDistances = _caps.limits.maxClipDistances; 
        info.limits.maxCullDistances = _caps.limits.maxCullDistances; 
        info.limits.maxCombinedClipAndCullDistances = _caps.limits.maxCombinedClipAndCullDistances; 
        //info.limits.pointSizeRange[0] = _caps.limits.pointSizeRange[0]; 
        //info.limits.pointSizeRange[1] = _caps.limits.pointSizeRange[1]; 
        //info.limits.lineWidthRange[0] = _caps.limits.lineWidthRange[0]; 
        //info.limits.lineWidthRange[1] = _caps.limits.lineWidthRange[1]; 
        info.limits.pointSizeGranularity = _caps.limits.pointSizeGranularity; 
        info.limits.lineWidthGranularity = _caps.limits.lineWidthGranularity;

        auto generalMSAASampleCntBits = _caps.limits.framebufferColorSampleCounts   //Render target supported
                                      & _caps.limits.sampledImageColorSampleCounts  //Sampled support
                                      & _caps.limits.storageImageSampleCounts       //Storage support
                                      ;
        info.limits.maxMSAASampleCount = _VkSampleCnts2AlSampleCnt(generalMSAASampleCntBits);
        
        //Will update in PopulateAdpFeatures
        info.limits.minStructuredBufferStride = _caps.SupportScalarBlockLayout() ? 4 : 16;
    }

    void VulkanAdapter::PopulateAdpFeatures() {
        _caps.ReadFromDevice(*_ctx, _handle);
    }

    void VulkanAdapter::PopulateAdpMemSegs() {
        auto& fnTable = _ctx->GetFnTable();
        info.memSegments.clear();
        //Evaluate vram size
        VkPhysicalDeviceMemoryProperties vramProp{};
        fnTable.vkGetPhysicalDeviceMemoryProperties(_handle, &vramProp);

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

        const static std::unordered_set<std::string> requiredExtensions = {
            VkDevExtNames::VK_KHR_SYNCHRONIZATION2,
            VkDevExtNames::VK_KHR_TIMELINE_SEMAPHORE,
            VkDevExtNames::VK_KHR_DYNAMIC_RENDERING,
            VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME,
            VkDevExtNames::VK_KHR_DEPTH_STENCIL_RESOLVE,
            VkDevExtNames::VK_KHR_CREATE_RENDERPASS2,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VkDevExtNames::VK_KHR_MAINTENANCE1,
        };

        auto _IsAdpSupportRequiredExts = [&](VkPhysicalDevice adp)->bool {
            //Does device supports required extensions
            uint32_t extensionCount;
            _fnTable.vkEnumerateDeviceExtensionProperties(adp, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            _fnTable.vkEnumerateDeviceExtensionProperties(adp, nullptr, &extensionCount, availableExtensions.data());

            uint32_t reqExtFoundCount = 0;
            
            for (const auto& extension : availableExtensions) {
                if(requiredExtensions.contains(extension.extensionName)) {
                    reqExtFoundCount++;
                }
            }

            return reqExtFoundCount == requiredExtensions.size();
        };

        std::uint32_t devCnt;
        VK_CHECK(_fnTable.vkEnumeratePhysicalDevices(_instance, &devCnt, nullptr));
        std::vector<VkPhysicalDevice> gpus(devCnt);
        VK_CHECK(_fnTable.vkEnumeratePhysicalDevices(_instance, &devCnt, gpus.data()));

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

    common::sp<VulkanContext> VulkanContext::Make(const IContext::Options& opts) {

        if(!VolkCtx::Ref()) {
            return nullptr;
        }

        uint32_t vkAPIVer = volkGetInstanceVersion();
        if(vkAPIVer < VK_API_VERSION_1_1) {
            std::cout << "Minimum supported Vulkan version is 1.1" << std::endl;
            return nullptr;
        }

        auto _ToSet = [](auto&& obj) {
            return std::unordered_set(obj.begin(), obj.end());
        };

        std::unordered_set<std::string> availableInstanceLayers{ _ToSet(EnumerateInstanceLayers()) };
        std::unordered_set<std::string> availableInstanceExtensions{ _ToSet(EnumerateInstanceExtensions()) };

       

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "";
        appInfo.applicationVersion = 0;
        appInfo.pEngineName = "alloy";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        //Support highest API version is 1.4
        appInfo.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        std::vector<const char*> instanceExtensions{};
        std::vector<const char*> instanceLayers{};

        auto _AddExtIfPresent = [&](const char* extName){
            if (availableInstanceExtensions.find(extName) 
                    != availableInstanceExtensions.end())
            {
                instanceExtensions.push_back(extName);
                return true;
            }
            return false;
        };

        auto _AddLayerIfPresent = [&](const char* layerName) {
            if (availableInstanceLayers.find(layerName)
                    != availableInstanceLayers.end())
            {
                instanceLayers.push_back(layerName);
                return true;
            }
            return false;
        };

        Capabilities instCaps {};

        instCaps.hasSurfaceExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_SURFACE);
        //Surface extensions
    #ifdef VLD_PLATFORM_WIN32
        instCaps.hasWin32SurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_WIN32_SURFACE);
    #elif defined VLD_PLATFORM_ANDROID
        instCaps.hasAndroidSurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_ANDROID_SURFACE);
    #elif defined VLD_PLATFORM_LINUX
        instCaps.hasXLibSurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_XLIB_SURFACE);
        instCaps.hasWaylandSurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_WAYLAND_SURFACE);
    #endif

        instCaps.hasDrvProp2Ext = _AddExtIfPresent(VkInstExtNames::VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES2);

        //Debugging
        if(opts.debug) {
            instCaps.hasDebugReportExt 
                = _AddExtIfPresent(VkInstExtNames::VK_EXT_DEBUG_REPORT);
            instCaps.hasStandardValidationLayers 
                = _AddLayerIfPresent(VkCommonStrings::StandardValidationLayerName);
            instCaps.hasKhronosValidationLayers 
                = _AddLayerIfPresent(VkCommonStrings::KhronosValidationLayerName);
        }

        instCaps.hasDebugUtilExt = _AddExtIfPresent(VkInstExtNames::VK_EXT_DEBUG_UTILS);

        createInfo.enabledExtensionCount = instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();

        createInfo.enabledLayerCount = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();

        VkInstance instance;
        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

        auto pCtx = new VulkanContext(instance);
        pCtx->_caps = std::move(instCaps);
        pCtx->_debugCallbackHandle = nullptr;
        volkLoadInstanceTable(&pCtx->_fnTable, instance);

        if (instCaps.hasDebugReportExt) {
            //Debug.WriteLine("Enabling Vulkan Debug callbacks.");
            VkDebugReportCallbackCreateInfoEXT debugCallbackCI{};
            debugCallbackCI.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debugCallbackCI.pNext = nullptr;
            debugCallbackCI.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
            debugCallbackCI.pfnCallback = &_DbgCbStatic;
            debugCallbackCI.pUserData = pCtx;

            VK_CHECK(pCtx->_fnTable.vkCreateDebugReportCallbackEXT(
                instance, &debugCallbackCI, nullptr, &pCtx->_debugCallbackHandle));       
        }

        return common::sp {pCtx};
    }


    VkBool32 VulkanContext::_DbgCbStatic(
        VkDebugReportFlagsEXT                       msgFlags,
        VkDebugReportObjectTypeEXT                  objectType,
        uint64_t                                    object,
        size_t                                      location,
        int32_t                                     msgCode,
        const char* pLayerPrefix,
        const char* pMsg,
        void* pUserData
    ) {
        auto self = (VulkanContext*)pUserData;
        
        return self->_DbgCb(msgFlags, objectType, object, location, msgCode, pLayerPrefix, pMsg);
    }
        

    VkBool32 VulkanContext::_DbgCb(
        VkDebugReportFlagsEXT                       msgFlags,
        VkDebugReportObjectTypeEXT                  objectType,
        uint64_t                                    object,
        size_t                                      location,
        int32_t                                     msgCode,
        const char* pLayerPrefix,
        const char* pMsg
    ) {
//#define LOG(...) printf(__VA_ARGS__)

        if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
            std::cout << std::format("ERR " "[{}] [Vk#{}]: {}", std::string(pLayerPrefix), msgCode, std::string(pMsg)) << std::endl;
        else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
            std::cout << std::format("WARN" "[{}] [Vk#{}]: {}", std::string(pLayerPrefix), msgCode, std::string(pMsg)) << std::endl;
        else if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
            std::cout << std::format("DBG " "[{}] [Vk#{}]: {}", std::string(pLayerPrefix), msgCode, std::string(pMsg)) << std::endl;
        else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
            std::cout << std::format("PERF" "[{}] [Vk#{}]: {}", std::string(pLayerPrefix), msgCode, std::string(pMsg)) << std::endl;
        else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
            std::cout << std::format("INFO" "[{}] [Vk#{}]: {}", std::string(pLayerPrefix), msgCode, std::string(pMsg)) << std::endl;
        return 0;
//#undef LOG
    }


} // namespace alloy::vk

