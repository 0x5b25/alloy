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
#include "VkStructStream.hpp"
#include "VulkanContext.hpp"
#include "VulkanCommandList.hpp"
//#include "VulkanDescriptorHeap.hpp"
#include "VulkanSwapChain.hpp"
//#include "VulkanResourceBarrier.hpp"

namespace alloy::vk {

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

        // Universal T2 heap layouts. All descriptor heaps that referenced these hold a
        // strong ref to this device, so they are already destroyed by now.
        if(_t2ResourceHeapDsl != VK_NULL_HANDLE)
            _fnTable.vkDestroyDescriptorSetLayout(_dev, _t2ResourceHeapDsl, nullptr);
        if(_t2SamplerHeapDsl != VK_NULL_HANDLE)
            _fnTable.vkDestroyDescriptorSetLayout(_dev, _t2SamplerHeapDsl, nullptr);
        if(_t2OffsetUBODSL != VK_NULL_HANDLE)
            _fnTable.vkDestroyDescriptorSetLayout(_dev, _t2OffsetUBODSL, nullptr);

        //Uninit managers
        _descPoolMgrs.clear();
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

    
    uint32_t VulkanDevice::_QueryMaxSupportedSamplerDescPerSet() {

        auto& devCaps = _adp->GetCaps();
        auto maxSetCnt = devCaps.properties12.maxUpdateAfterBindDescriptorsInAllPools;
        
        {
            // Provided by VK_VERSION_1_2
            VkDescriptorSetVariableDescriptorCountLayoutSupport varCntSupport {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT,
                //uint32_t           maxVariableDescriptorCount;
            };

            VkDescriptorSetLayoutSupport supportStat {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,
                .pNext = &varCntSupport,
            };

            VkDescriptorSetLayoutBinding binding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                               | VK_SHADER_STAGE_VERTEX_BIT
                               | VK_SHADER_STAGE_FRAGMENT_BIT
                               | VK_SHADER_STAGE_TASK_BIT_EXT
                               | VK_SHADER_STAGE_MESH_BIT_EXT
                               , // Add raytracing in the future
            };

            
            static constexpr VkDescriptorBindingFlags bindingFlags =
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;


            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 1,
                .pBindingFlags = &bindingFlags,
            };

            VkDescriptorSetLayoutCreateInfo dslCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &bindingFlagsCI,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = 1,
                .pBindings = &binding,
            };

            _fnTable.vkGetDescriptorSetLayoutSupport(_dev, &dslCI, &supportStat);

            if(supportStat.supported) {
                maxSetCnt = varCntSupport.maxVariableDescriptorCount;
            }
        };

        return std::min({ 
            maxSetCnt,
            devCaps.properties12.maxUpdateAfterBindDescriptorsInAllPools,
            
            // Sampler heap
            devCaps.properties12.maxDescriptorSetUpdateAfterBindSamplers,
            devCaps.properties12.maxPerStageDescriptorUpdateAfterBindSamplers,
        });
    }
    
    uint32_t VulkanDevice::_QueryMaxSupportedMutDescPerSet() {

        auto& devCaps = _adp->GetCaps();
        auto maxSetCnt = devCaps.properties12.maxUpdateAfterBindDescriptorsInAllPools;

        {
            
            // Provided by VK_VERSION_1_2
            VkDescriptorSetVariableDescriptorCountLayoutSupport varCntSupport {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT,
                //uint32_t           maxVariableDescriptorCount;
            };

            VkDescriptorSetLayoutSupport supportStat {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,
                .pNext = &varCntSupport,
            };

            VkDescriptorSetLayoutBinding binding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                               | VK_SHADER_STAGE_VERTEX_BIT
                               | VK_SHADER_STAGE_FRAGMENT_BIT
                               | VK_SHADER_STAGE_TASK_BIT_EXT
                               | VK_SHADER_STAGE_MESH_BIT_EXT
                               , // Add raytracing in the future
            };

            
            static const VkDescriptorType mutableTypes [] = {
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            };
            static constexpr VkDescriptorBindingFlags bindingFlags =
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

            VkMutableDescriptorTypeListEXT mutableTypeList {
                .descriptorTypeCount = sizeof(mutableTypes) / sizeof(mutableTypes[0]),
                .pDescriptorTypes = mutableTypes
            };

            VkMutableDescriptorTypeCreateInfoEXT mutableCI {
                .sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
                .mutableDescriptorTypeListCount = 1,
                .pMutableDescriptorTypeLists = &mutableTypeList
            };


            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = &mutableCI,
                .bindingCount = 1,
                .pBindingFlags = &bindingFlags,
            };

            VkDescriptorSetLayoutCreateInfo dslCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &bindingFlagsCI,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = 1,
                .pBindings = &binding,
            };

            _fnTable.vkGetDescriptorSetLayoutSupport(_dev, &dslCI, &supportStat);

            if(supportStat.supported) {
                maxSetCnt = varCntSupport.maxVariableDescriptorCount;
            }
        };

        
        // https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/vkGetDescriptorSetLayoutSupport.html:
        // Conservative size is VkPhyDev1.1 maintenance3 props
        // VkPhysicalDeviceVulkan11Properties::maxPerSetDescriptors
        
        return std::min( { 
            maxSetCnt,
            // Combined resoource heap
            devCaps.properties12.maxDescriptorSetUpdateAfterBindUniformBuffers,
            //devCaps.properties12.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic;
            devCaps.properties12.maxDescriptorSetUpdateAfterBindStorageBuffers,
            //devCaps.properties12.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
            devCaps.properties12.maxDescriptorSetUpdateAfterBindSampledImages,
            devCaps.properties12.maxDescriptorSetUpdateAfterBindStorageImages,

            devCaps.properties12.maxPerStageDescriptorUpdateAfterBindUniformBuffers,
            devCaps.properties12.maxPerStageDescriptorUpdateAfterBindStorageBuffers,
            devCaps.properties12.maxPerStageDescriptorUpdateAfterBindSampledImages,
            devCaps.properties12.maxPerStageDescriptorUpdateAfterBindStorageImages,
            devCaps.properties12.maxPerStageUpdateAfterBindResources,
        });
    }

    void VulkanDevice::_CreateT2DSLs() {
        // Shared binding flags for both heap DSLs: the heap is a partially-populated,
        // update-after-bind, variable-count array.
        static constexpr VkDescriptorBindingFlags defaultBindingFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
           // VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

        auto _CreateDSL = [&](
            VkDescriptorType type,
            uint32_t count,
            VkDescriptorBindingFlags bindingFlags,
            const VkMutableDescriptorTypeCreateInfoEXT* mutableCI
        ) -> VkDescriptorSetLayout {
            VkDescriptorSetLayoutBinding binding{
                .binding = 0,
                .descriptorType = type,
                .descriptorCount = count,
                .stageFlags = VK_SHADER_STAGE_ALL,
            };

            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = mutableCI,
                .bindingCount = 1,
                .pBindingFlags = &bindingFlags,
            };

            VkDescriptorSetLayoutCreateInfo dslCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &bindingFlagsCI,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = 1,
                .pBindings = &binding,
            };

            VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
            VK_CHECK(VK_DEV_CALL(this,
                vkCreateDescriptorSetLayout(_dev, &dslCI, nullptr, &dsl)));
            return dsl;
        };

        // Resource heap: mutable-typed, candidate types match _QueryMaxSupportedMutDescPerSet.

        VkMutableDescriptorTypeListEXT mutableTypeList{
            .descriptorTypeCount = sizeof(_DescriptorPoolMgr::kMutableDescTypes) 
                / sizeof(_DescriptorPoolMgr::kMutableDescTypes[0]),
            .pDescriptorTypes = _DescriptorPoolMgr::kMutableDescTypes,
        };

        VkMutableDescriptorTypeCreateInfoEXT mutableCI{
            .sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
            .mutableDescriptorTypeListCount = 1,
            .pMutableDescriptorTypeLists = &mutableTypeList,
        };

        _t2ResourceHeapDsl = _CreateDSL(
            VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
            _features.maxVariableCntDescriptorsPerSetMutableType,
            defaultBindingFlags | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            &mutableCI);

        _t2SamplerHeapDsl = _CreateDSL(
            VK_DESCRIPTOR_TYPE_SAMPLER,
            _features.maxVariableCntDescriptorsPerSetSampler,
            defaultBindingFlags | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            nullptr);
        
        _t2OffsetUBODSL = _CreateDSL(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            defaultBindingFlags,
            1,
            nullptr
        );
    }

    common::sp<IGraphicsDevice> VulkanDevice::Make(
		const common::sp<VulkanAdapter>& adp,
		const IGraphicsDevice::Options& options
	){
        auto& ctx = adp->GetCtx();
        auto& devCaps = adp->GetCaps();

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
        dev->_features.flags.hasComputeCap = qInfo.graphicsQueueSupportsCompute;

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

            dev->_features.flags.hasUniqueCpyQueue = true;
        }

        if (qInfo.computeQueueFamily.has_value()) {
            queueCreateInfos.emplace_back();
            VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = qInfo.computeQueueFamily.value();
            queueCreateInfo.queueCount = 1;

            queuePriorities.push_back(0.6f);

            dev->_features.flags.hasUniqueComputeQueue = true;
        }

        for (auto i = 0; i < queueCreateInfos.size(); i++) {
            queueCreateInfos[i].pQueuePriorities = &queuePriorities[i];
        }


        //Requested device extensions
        //First, get all available extensions
        std::unordered_set<std::string> availableDevExts{};
        {

            std::uint32_t propCount = 0;
            VK_CHECK(ctx.GetFnTable().vkEnumerateDeviceExtensionProperties(adp->GetHandle(), nullptr, &propCount, nullptr));

            if (propCount != 0) {
                std::vector<VkExtensionProperties> props(propCount);
                VK_CHECK(ctx.GetFnTable().vkEnumerateDeviceExtensionProperties(
                    adp->GetHandle(), nullptr, &propCount, props.data()));


                for (int i = 0; i < propCount; i++)
                {
                    auto& prop = props[i];
                    availableDevExts.emplace(prop.extensionName);
                }
            }
        }

        std::vector<const char*> devExtensions{
            //Prefill required extensions

            VkDevExtNames::VK_KHR_TIMELINE_SEMAPHORE,

            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VkDevExtNames::VK_KHR_MAINTENANCE1,
        };

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        //First add pointers to the queue creation info and device features structs:
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = queueCreateInfos.size();

        VkPhysicalDeviceFeatures deviceFeatures{};
        ctx.GetFnTable().vkGetPhysicalDeviceFeatures(adp->GetHandle(), &deviceFeatures);

        StructStream featureStructs{};

        auto& features11 = featureStructs.Append<VkPhysicalDeviceVulkan11Features,
                                                 VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES>();

        features11.shaderDrawParameters = true; //Not sure, required by dxil-spirv

        auto& features12 = featureStructs.Append<VkPhysicalDeviceVulkan12Features,
                                                 VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES>();

        features12.timelineSemaphore = true;
        features12.separateDepthStencilLayouts = true;

        //Bindless shader ABI and binding models
        {
            //Enable the resource heap indexing
            if(devCaps.resourceBindingModel != ResourceBindingModel::FixedBindings) {
                features12.descriptorIndexing = true;
                features12.descriptorBindingPartiallyBound = true;
                features12.descriptorBindingUpdateUnusedWhilePending = true;
                features12.descriptorBindingUniformBufferUpdateAfterBind = true;
                features12.descriptorBindingSampledImageUpdateAfterBind = true;
                features12.descriptorBindingStorageImageUpdateAfterBind = true;
                features12.descriptorBindingStorageBufferUpdateAfterBind = true;
                features12.runtimeDescriptorArray = true;
                deviceFeatures.shaderUniformBufferArrayDynamicIndexing = true;
                deviceFeatures.shaderSampledImageArrayDynamicIndexing = true;
                deviceFeatures.shaderStorageBufferArrayDynamicIndexing = true;
                deviceFeatures.shaderStorageImageArrayDynamicIndexing = true;
                features12.shaderUniformTexelBufferArrayDynamicIndexing = true;
                features12.shaderStorageTexelBufferArrayDynamicIndexing = true;

                if(devCaps.SupportNonUniformResourceIndexing()) {
                    features12.shaderUniformBufferArrayNonUniformIndexing = true;
                    features12.shaderSampledImageArrayNonUniformIndexing = true;
                    features12.shaderStorageBufferArrayNonUniformIndexing = true;
                    features12.shaderStorageImageArrayNonUniformIndexing = true;
                    features12.shaderUniformTexelBufferArrayNonUniformIndexing = true;
                    features12.shaderStorageTexelBufferArrayNonUniformIndexing = true;
                }

                if(devCaps.resourceBindingModel == ResourceBindingModel::T2) {
                    features12.descriptorBindingVariableDescriptorCount = true;
                }
            }

            //Enable the descriptor buffer
            if(devCaps.supportDescriptorBuffer) {
                devExtensions.push_back(VkDevExtNames::VK_EXT_DESCRIPTOR_BUFFER);
                features12.bufferDeviceAddress = true;

                auto& descriptorBufferFeatures
                    = featureStructs.Append<VkPhysicalDeviceDescriptorBufferFeaturesEXT,
                                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT>();
                descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
            }

            //Mutable desc types
            if(devCaps.supportMutableDescriptorType) {
                devExtensions.push_back(VkDevExtNames::VK_EXT_MUTABLE_DESCRIPTOR_TYPE);

                auto& mutableDescriptorTypeFeatures
                    = featureStructs.Append<VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT,
                                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT>();
                mutableDescriptorTypeFeatures.mutableDescriptorType = VK_TRUE;
            }
        }

        if(devCaps.SupportScalarBlockLayout()) {
            features12.scalarBlockLayout = true;
        }

        //VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures { };
        //scalarBlockLayoutFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
        //scalarBlockLayoutFeatures.scalarBlockLayout = VK_TRUE;
        //features12.pNext = &scalarBlockLayoutFeatures;
        {
            devExtensions.push_back(VkDevExtNames::VK_KHR_DYNAMIC_RENDERING);
            //Companion extensions required by dynamic rendering
            devExtensions.push_back(VkDevExtNames::VK_KHR_DEPTH_STENCIL_RESOLVE);
            devExtensions.push_back(VkDevExtNames::VK_KHR_CREATE_RENDERPASS2);

            auto& dynamicRenderingFeature
                = featureStructs.Append<VkPhysicalDeviceDynamicRenderingFeaturesKHR,
                                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR>();
            dynamicRenderingFeature.dynamicRendering = VK_TRUE;

            devExtensions.push_back(VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME);
            auto& dynRndrUnusedAttachments
                = featureStructs.Append<VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT,
                                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT>();
            dynRndrUnusedAttachments.dynamicRenderingUnusedAttachments = VK_TRUE;
        }

        {
            devExtensions.push_back(VkDevExtNames::VK_KHR_SYNCHRONIZATION2);
            auto& sync2Features
                = featureStructs.Append<VkPhysicalDeviceSynchronization2Features,
                                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES>();
            sync2Features.synchronization2 = VK_TRUE;
        }


        if(devCaps.SupportMeshShader()) {
            devExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
            auto& meshShaderFeature
                = featureStructs.Append<VkPhysicalDeviceMeshShaderFeaturesEXT,
                                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT>();

            meshShaderFeature.taskShader = VK_TRUE;
            meshShaderFeature.meshShader = VK_TRUE;
        }

        createInfo.pNext = featureStructs.Front<void*>();

        auto _AddExtIfPresent = [&](const char* extName) {
            if (availableDevExts.contains(extName))
            {
                devExtensions.push_back(extName);
                return true;
            }
            return false;
        };

        //if(!_AddExtIfPresent(VkDevExtNames::VK_KHR_TIMELINE_SEMAPHORE)) {
        //    throw std::exception("VkDevice creation failed. Timeline semaphore not supported");
        //}
        //
        //if(!_AddExtIfPresent(VkDevExtNames::VK_KHR_DYNAMIC_RENDERING) ||
        //   !_AddExtIfPresent(VkDevExtNames::VK_KHR_DEPTH_STENCIL_RESOLVE) ||
        //   !_AddExtIfPresent(VkDevExtNames::VK_KHR_CREATE_RENDERPASS2)
        //) {
        //    throw std::exception("VkDevice creation failed. dynamic rendering not supported");
        //}
        //
        //if(!_AddExtIfPresent(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
        //    throw std::exception("VkDevice creation failed. synchronization2 not supported");
        //}


        //if (options.debug)
        //    dev->_features.supportsDebug = _AddExtIfPresent(VkDevExtNames::VK_EXT_DEBUG_UTILS);

        dev->_features.flags.supportsPresent = _AddExtIfPresent(VkDevExtNames::VK_KHR_SWAPCHAIN);
        //if (options.preferStandardClipSpaceYDirection) {
        //    dev->_features.flags.supportsMaintenance1 = _AddExtIfPresent(VkDevExtNames::VK_KHR_MAINTENANCE1);
        //}
        dev->_features.flags.supportsMaintenance1 = true; // VK1.1 core behavior, no extension needed.
        dev->_features.flags.supportsGetMemReq2 = _AddExtIfPresent(VkDevExtNames::VK_KHR_GET_MEMORY_REQ2);
        dev->_features.flags.supportsDedicatedAlloc = _AddExtIfPresent(VkDevExtNames::VK_KHR_DEDICATED_ALLOCATION);

        if (ctx.GetCaps().hasDrvProp2Ext) {
            dev->_features.flags.supportsDrvPropQuery = _AddExtIfPresent(VkDevExtNames::VK_KHR_DRIVER_PROPS);
        }

        dev->_features.flags.supportsDepthClip = _AddExtIfPresent(VkDevExtNames::VK_EXT_DEPTH_CLIP_ENABLE);
        dev->_features.flags.supportReadOnlyAttachment = _AddExtIfPresent(VK_KHR_LOAD_STORE_OP_NONE_EXTENSION_NAME);

        createInfo.enabledExtensionCount = static_cast<uint32_t>(devExtensions.size());
        createInfo.ppEnabledExtensionNames = devExtensions.data();

        createInfo.pEnabledFeatures = &deviceFeatures;
        VK_CHECK(ctx.GetFnTable().vkCreateDevice(adp->GetHandle(), &createInfo, nullptr, &dev->_dev));

        volkLoadDeviceTable(&dev->_fnTable, dev->_dev);

        //Create command pool
        //VkCommandPoolCreateInfo poolInfo{};
        //poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        //poolInfo.queueFamilyIndex = devInfo.graphicsQueueFamily;
        //poolInfo.flags = 0; // Optional
        //VK_CHECK(vkCreateCommandPool(dev->_dev, &poolInfo, nullptr, &dev->_cmdPool));
        //dev->_cmdPoolMgr.Init(dev->_dev, devInfo.graphicsQueueFamily);

        auto _CreatePoolMgr = [&](VkDescriptorType type) {
            
            dev->_descPoolMgrs.emplace(
                type, _DescriptorPoolMgr::CreateArgs{
                    .dev = dev.get(),
                    .type = type,
                    .maxSetsPerPool = 128,
                    .maxDescriptorsPerPool = 1024
                }
            );
        };

        _CreatePoolMgr(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _CreatePoolMgr(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _CreatePoolMgr(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _CreatePoolMgr(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        _CreatePoolMgr(VK_DESCRIPTOR_TYPE_SAMPLER);

        dev->_features.maxVariableCntDescriptorsPerSetSampler =
             dev->_QueryMaxSupportedSamplerDescPerSet();

        if(devCaps.supportMutableDescriptorType) {
            _CreatePoolMgr(VK_DESCRIPTOR_TYPE_MUTABLE_EXT);
            dev->_features.maxVariableCntDescriptorsPerSetMutableType =
                dev->_QueryMaxSupportedMutDescPerSet();
            // Universal T2 heap layouts depend on maxMutableDescriptorsPerSet being set.
            dev->_CreateT2DSLs();
        } else {
            dev->_features.maxVariableCntDescriptorsPerSetMutableType = 0;
        }

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
        auto apiVer = adp->GetAdapterInfo().apiVersion;

        //Init allocator
        VmaAllocatorCreateInfo allocatorInfo = {};
        if(devCaps.supportDescriptorBuffer) {
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }
        allocatorInfo.vulkanApiVersion = VK_MAKE_API_VERSION(0, apiVer.major, apiVer.minor, apiVer.patch);
        allocatorInfo.physicalDevice = adp->GetHandle();
        allocatorInfo.device = dev->_dev;
        allocatorInfo.instance = adp->GetCtx().GetHandle();

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
        if (dev->_features.flags.supportsGetMemReq2 && dev->_features.flags.supportsDedicatedAlloc) {
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
        ctx.GetFnTable().vkGetPhysicalDeviceProperties(adp->GetHandle(), &deviceProps);

        dev->_commonFeat.computeShader = dev->_features.flags.hasComputeCap;
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
        dev->_commonFeat.commandListDebugMarkers = dev->_features.flags.supportsDebug;
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


    ISwapChain::State VulkanDevice::PresentToSwapChain(
        ISwapChain* sc
    ) {

        VulkanSwapChain* vkSC = PtrCast<VulkanSwapChain>(sc);
        //auto tex = vkSC->GetCurrentColorTarget()->GetTextureObject().get();

        auto sem = _gfxQ->PrepareForPresent();

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

    _DescriptorSet VulkanDevice::AllocateDescriptorSet(VkDescriptorSetLayout layout,
            VkDescriptorType type,
            uint32_t descriptorCnt, // Total descriptor counts. We can't get this info from
                                //   VkDescriptorSetLayout.
            bool isVariableCnt, // Enables variable count
            bool isMutableSet   // Implies partially bound and update after use.
    ){
        assert(_descPoolMgrs.contains(type));
        assert(!isMutableSet ||
           GetAdapter().GetAdapterInfo().resourceBindingModel
               != ResourceBindingModel::FixedBindings
            && "Vulkan mutable ResourceSet requires DescriptorIndexing support.");

        return _descPoolMgrs.at(type).Allocate(
            layout,
            descriptorCnt,
            isVariableCnt
        );
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
        case HostAccess::SystemMemoryPreferWrite:
            //Assume non-cached write combine
            allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        case HostAccess::SystemMemoryPreferRead:
            //Not necessarily coherent, flush/invalidate might be needed
            allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
        case HostAccess::PreferDeviceMemory:
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


        createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        VK_CHECK(VK_DEV_CALL(dev,
            vkCreateSemaphore(dev->LogicalDev(), &createInfo, nullptr, &_presentFence)));
    }

    VulkanCommandQueue::~VulkanCommandQueue() {
        VK_DEV_CALL(_dev,vkDestroySemaphore(_dev->LogicalDev(), _presentFence, nullptr));
    }

    void VulkanCommandQueue::EncodeSignalEvent(IEvent* evt, uint64_t value) {
        auto vkFence = PtrCast<VulkanFence>(evt);

        auto rawFence = vkFence->GetHandle();
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
        auto vkFence = PtrCast<VulkanFence>(evt);

        auto rawFence = vkFence->GetHandle();

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

    VkSemaphore VulkanCommandQueue::PrepareForPresent() {
        // We need a semaphore to mimc DX12 queue and present sync.
        // Vulkan timeline semaphores currently can't sync between
        // presentation engine and command queues. So we use a 
        // VkSemaphore
        {
            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &_presentFence;

            VK_DEV_CALL(_dev, vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE));
        }

        return _presentFence;
    }

    void VulkanCommandQueue::SubmitCommand(ICommandList* cmd) {

        assert(cmd != nullptr);
        auto* vkCmd = PtrCast<VulkanCommandList>(cmd);

        auto rawCmdBuf = vkCmd->GetHandle();

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &rawCmdBuf;
        submitInfo.signalSemaphoreCount  = 0;
        submitInfo.pSignalSemaphores = nullptr;

        VK_CHECK(VK_DEV_CALL(_dev, vkQueueSubmit(
            _q, 1, &submitInfo, nullptr
        )));
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
