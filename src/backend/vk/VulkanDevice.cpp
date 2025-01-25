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
#include "VulkanCommandList.hpp"
#include "VulkanSwapChain.hpp"


namespace alloy {
    
	common::sp<IGraphicsDevice> CreateVulkanGraphicsDevice(
        const IGraphicsDevice::Options& options
    ) {
		return alloy::vk::VulkanDevice::Make(options);
	}
}

namespace alloy::vk {

template<typename T, typename U>
std::unordered_set<T> ToSet(U&& obj) {
    return std::unordered_set<T>(obj.begin(), obj.end());
}

template<typename T, typename U>
bool Contains(T&& container, U&& element) {

    return container.find(element) != container.end();
}

class _VkCtx : public alloy::common::NVRefCnt< _VkCtx>{

public:
    union Features
    {
        struct {
            bool hasSurfaceExt : 1;
            bool hasWin32SurfaceCreateExt : 1;
            bool hasAndroidSurfaceCreateExt : 1;
            bool hasXLibSurfaceCreateExt : 1;
            bool hasWaylandSurfaceCreateExt : 1;

            bool hasDrvProp2Ext : 1;

            bool hasDebugReportExt : 1;
            bool hasStandardValidationLayers : 1;
            bool hasKhronosValidationLayers : 1;

        };
        std::uint32_t value;
    };

private:

    bool _isVkLoaded;
    VkInstance _instance;
    Features _features;

    VkDebugReportCallbackEXT _debugCallbackHandle;

    alloy::GraphicsApiVersion _ver;
    
    //Mechanism for thread-safe singleton
    static std::atomic<_VkCtx*> _pinstance;
    static std::mutex _m;

    _VkCtx():
        _isVkLoaded(false),
        _instance(VK_NULL_HANDLE),
        _features({ 0 }),
        _debugCallbackHandle(VK_NULL_HANDLE),
        _ver({0})
    {
        if (volkInitialize() != VK_SUCCESS) {
            return;
        }

        _isVkLoaded = true;

        std::unordered_set<std::string> availableInstanceLayers{ ToSet<std::string>(EnumerateInstanceLayers()) };
        std::unordered_set<std::string> availableInstanceExtensions{ ToSet<std::string>(EnumerateInstanceExtensions()) };

        uint32_t vkAPIVer = 0;
        vkEnumerateInstanceVersion(&vkAPIVer);

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        std::vector<const char*> instanceExtensions{};
        std::vector<const char*> instanceLayers{};

        auto _AddExtIfPresent = [&](const char* extName){
            if (Contains(availableInstanceExtensions, extName))
            {
                instanceExtensions.push_back(extName);
                return true;
            }
            return false;
        };

        auto _AddLayerIfPresent = [&](const char* layerName) {
            if (Contains(availableInstanceLayers, layerName))
            {
                instanceLayers.push_back(layerName);
                return true;
            }
            return false;
        };

        _features.hasSurfaceExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_SURFACE);
        //Surface extensions
    #ifdef VLD_PLATFORM_WIN32
        _features.hasWin32SurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_WIN32_SURFACE);
    #elif defined VLD_PLATFORM_ANDROID
        _features.hasAndroidSurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_ANDROID_SURFACE);
    #elif defined VLD_PLATFORM_LINUX
        _features.hasXLibSurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_XLIB_SURFACE);
        _features.hasWaylandSurfaceCreateExt = _AddExtIfPresent(VkInstExtNames::VK_KHR_WAYLAND_SURFACE);
    #endif

        _features.hasDrvProp2Ext = _AddExtIfPresent(VkInstExtNames::VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES2);

        //Debugging
    #ifdef VLD_DEBUG
        _features.hasDebugReportExt 
            = _AddExtIfPresent(VkInstExtNames::VK_EXT_DEBUG_REPORT);
        _features.hasStandardValidationLayers 
            = _AddLayerIfPresent(VkCommonStrings::StandardValidationLayerName);
        _features.hasStandardValidationLayers 
            = _AddLayerIfPresent(VkCommonStrings::KhronosValidationLayerName);
    #endif  
        
        createInfo.enabledExtensionCount = instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();

        createInfo.enabledLayerCount = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();

        assert(vkCreateInstance(&createInfo, nullptr, &_instance) == VK_SUCCESS);
        volkLoadInstance(_instance);

        auto version = volkGetInstanceVersion();
        _ver.major = VK_VERSION_MAJOR(version);
        _ver.minor = VK_VERSION_MINOR(version);
        _ver.patch = VK_VERSION_PATCH(version);


        if (_features.hasDebugReportExt) {
            _EnableDebugCallback();
        }
    }


    static VkBool32 _DbgCb(
        VkDebugReportFlagsEXT                       msgFlags,
        VkDebugReportObjectTypeEXT                  objectType,
        uint64_t                                    object,
        size_t                                      location,
        int32_t                                     msgCode,
        const char* pLayerPrefix,
        const char* pMsg,
        void* pUserData)
    {
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


    void _EnableDebugCallback(
        VkDebugReportFlagsEXT flags =
        VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT
    ) {
        //Debug.WriteLine("Enabling Vulkan Debug callbacks.");
        VkDebugReportCallbackCreateInfoEXT debugCallbackCI{};
        debugCallbackCI.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debugCallbackCI.pNext = nullptr;
        debugCallbackCI.flags = flags;
        debugCallbackCI.pfnCallback = &_DbgCb;
        
        VK_CHECK(vkCreateDebugReportCallbackEXT(
            _instance, &debugCallbackCI, nullptr, &_debugCallbackHandle));            
    }

public:

    ~_VkCtx() {
        //Presumably non-multithreaded
        _pinstance = nullptr;
        if (!_isVkLoaded) return;

        if (_debugCallbackHandle != VK_NULL_HANDLE) {
            vkDestroyDebugReportCallbackEXT(_instance, _debugCallbackHandle, nullptr);
        }

        vkDestroyInstance(_instance, nullptr);
    }

    static common::sp<_VkCtx> Get() {
        if (_pinstance == nullptr) {
            std::lock_guard<std::mutex> lock{ _m };
            if (_pinstance == nullptr) {
                _pinstance = new _VkCtx();
            }
        } else {
            _pinstance.load()->ref();
        }
        return common::sp{ _pinstance.load()};
    }

    const VkInstance& GetHandle() const { return _instance; }

    bool IsVulkanSupported() {
        return _instance != VK_NULL_HANDLE;
    }

    const Features& GetFeatures() const { return _features; }
    const alloy::GraphicsApiVersion& GetApiVer() const { return _ver; }
};

std::atomic<_VkCtx*> _VkCtx::_pinstance{ nullptr };
std::mutex _VkCtx::_m{};

std::optional<PhyDevInfo> _PickPhysicalDevice(
    VkInstance inst,
    VkSurfaceKHR surface,
    const std::vector<std::string>& extensions
) {
    std::uint32_t devCnt;
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &devCnt, nullptr));
    std::vector<VkPhysicalDevice> gpus(devCnt);
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &devCnt, gpus.data()));

    //std::vector<VkPhysicalDevice> gpus =  context.instance.enumeratePhysicalDevices();
    std::vector<std::tuple<std::uint32_t, PhyDevInfo>> devList;

    auto _RatePhyDev = [&](VkPhysicalDevice phyDev, std::uint32_t& score, PhyDevInfo& info) {
        //Can device do geomerty shaders?
        VkPhysicalDeviceFeatures devFeat{};
        vkGetPhysicalDeviceFeatures(phyDev, &devFeat);
        if (!devFeat.geometryShader) return false;

        //Does device supports required extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(phyDev, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(phyDev, nullptr, &extensionCount, availableExtensions.data());

        std::unordered_set<std::string> requiredExtensions(extensions.begin(), extensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        if (!requiredExtensions.empty()) return false;

        //Find graphics queue
        std::uint32_t queueFamCnt;
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamCnt, nullptr);
        std::vector<VkQueueFamilyProperties> queue_family_properties(queueFamCnt);
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &queueFamCnt, queue_family_properties.data());

        if (queue_family_properties.empty())
        {
            return false;
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
                    return false;
                }
                //Add to exclude list
                excludes.push_back(res.value());

                //Check for presentation support
                if (surface != VK_NULL_HANDLE) {
                    VkBool32 supports_present;
                    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
                        phyDev, res.value(), surface, &supports_present)
                    );
                    if (!supports_present) {
                        break;
                    }
                }
                // Find a queue family which supports graphics and presentation.
                //if (supports_present)
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
            if (gQueueProp.queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT) {
                info.graphicsQueueSupportsCompute = true;
                score += 100;
            }
        }
        if (devProp.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;

        {
            //Try find dedicated compute queue
            auto res = _FindUniqueFamilyWithFlags(VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT, excludes);
            if (res.has_value()) {
                excludes.push_back(res.value());
                info.computeQueueFamily = res.value();
                score += 200;
            }
        }

        {
            //Try find dedicated transfer queue
            auto res = _FindUniqueFamilyWithFlags(VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT, excludes);
            if (res.has_value()) {
                excludes.push_back(res.value());
                info.transferQueueFamily = res.value();
                score += 100;
            }
        }
        //Evaluate vram size
        VkPhysicalDeviceMemoryProperties vramProp{};
        vkGetPhysicalDeviceMemoryProperties(phyDev, &vramProp);

        for (uint32_t i = 0; i < vramProp.memoryHeapCount; i++) {
            //Locate the first device local heap
            if (vramProp.memoryHeaps[i].flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                score += vramProp.memoryHeaps[i].size / (8ULL * 1024 * 1024);
                break;
            }
        }

        return true;
    };

    for (auto& gpu : gpus) {
        std::uint32_t score; PhyDevInfo info{};
        if (!_RatePhyDev(gpu, score, info)) {
            continue;
        }
        devList.push_back({ score, info });
    }

    if (devList.empty()) return {};
    auto max = 0, maxScore = 0;
    for (auto i = 0; i < devList.size(); i++) {
        auto score = std::get<uint32_t>(devList[i]);
        if (score >= maxScore) max = i, maxScore = score;
    }
    return std::get<PhyDevInfo>(devList[max]);
}


    VulkanDevice::VulkanDevice() {};

    VulkanDevice::~VulkanDevice()
    {
        vkDeviceWaitIdle(_dev);

        
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

        vkDestroyDevice(_dev, nullptr);

        DEBUGCODE(_ctx = nullptr);
        DEBUGCODE(_phyDev = {});
        DEBUGCODE(_dev = VK_NULL_HANDLE);
        DEBUGCODE(_allocator = VK_NULL_HANDLE);

        DEBUGCODE(_gfxQ = VK_NULL_HANDLE);
        DEBUGCODE(_copyQ = VK_NULL_HANDLE);
        DEBUGCODE(_computeQ = VK_NULL_HANDLE);

        //DEBUGCODE(_surface = VK_NULL_HANDLE);
    }

    common::sp<IGraphicsDevice> VulkanDevice::Make(
		const IGraphicsDevice::Options& options
	){
        auto ctx = _VkCtx::Get();
        if (!ctx->IsVulkanSupported()) {
            return {};
        }

        //Make the surface if possible
        //VK::priv::SurfaceContainer _surf = {VK_NULL_HANDLE, false};

        //if (swapChainSource != nullptr) {
        //    _surf = VK::priv::CreateSurface(ctx->GetHandle(), swapChainSource);
        //}

        auto phyDev = _PickPhysicalDevice(ctx->GetHandle(), nullptr/*_surf.surface*/, {});
        if (!phyDev.has_value()) {
            return {};
        }

        auto& devInfo = phyDev.value();

        auto dev = common::sp<VulkanDevice>(new VulkanDevice());
        dev->_ctx = ctx;
        //ev->_surface = _surf.surface;
        //dev->_isOwnSurface = _surf.isOwnSurface;
        dev->_phyDev = devInfo;
        dev->_features = {};
        dev->_features.hasComputeCap = devInfo.graphicsQueueSupportsCompute;

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

        if (devInfo.transferQueueFamily.has_value()) {
            queueCreateInfos.emplace_back();
            VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = devInfo.transferQueueFamily.value();
            queueCreateInfo.queueCount = 1;

            queuePriorities.push_back(0.8f);

            dev->_features.hasUniqueCpyQueue = true;
        }

        if (devInfo.computeQueueFamily.has_value()) {
            queueCreateInfos.emplace_back();
            VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.back();
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = devInfo.computeQueueFamily.value();
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
        vkGetPhysicalDeviceFeatures(dev->_phyDev.handle, &deviceFeatures);

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
            VK_CHECK(vkEnumerateDeviceExtensionProperties(dev->_phyDev.handle, nullptr, &propCount, nullptr));

            if (propCount != 0) {
                std::vector<VkExtensionProperties> props(propCount);
                VK_CHECK(vkEnumerateDeviceExtensionProperties(
                    dev->_phyDev.handle, nullptr, &propCount, props.data()));

                
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

        if (dev->_ctx->GetFeatures().hasDrvProp2Ext) {
            dev->_features.supportsDrvPropQuery = _AddExtIfPresent(VkDevExtNames::VK_KHR_DRIVER_PROPS);
        }

        dev->_features.supportsDepthClip = _AddExtIfPresent(VkDevExtNames::VK_EXT_DEPTH_CLIP_ENABLE);

        createInfo.enabledExtensionCount = static_cast<uint32_t>(devExtensions.size());
        createInfo.ppEnabledExtensionNames = devExtensions.data();

        createInfo.pEnabledFeatures = &deviceFeatures;
        VK_CHECK(vkCreateDevice(dev->_phyDev.handle, &createInfo, nullptr, &dev->_dev));

        //Create command pool
        //VkCommandPoolCreateInfo poolInfo{};
        //poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        //poolInfo.queueFamilyIndex = devInfo.graphicsQueueFamily;
        //poolInfo.flags = 0; // Optional
        //VK_CHECK(vkCreateCommandPool(dev->_dev, &poolInfo, nullptr, &dev->_cmdPool));
        //dev->_cmdPoolMgr.Init(dev->_dev, devInfo.graphicsQueueFamily);
        dev->_descPoolMgr.Init(dev->_dev, 1000);

        //Get queues
        {
            VkQueue rawGfxQ;
            vkGetDeviceQueue(dev->_dev, devInfo.graphicsQueueFamily, 0, &rawGfxQ);
            dev->_gfxQ = new VulkanCommandQueue(dev.get(), devInfo.graphicsQueueFamily, rawGfxQ);
        }
        if (devInfo.transferQueueFamily.has_value()) {
            VkQueue rawCopyQ;
            vkGetDeviceQueue(dev->_dev, devInfo.transferQueueFamily.value(), 0, &rawCopyQ);
            dev->_copyQ = new VulkanCommandQueue(dev.get(),
                                                 devInfo.transferQueueFamily.value(),
                                                 rawCopyQ);
        }
        if (devInfo.computeQueueFamily.has_value()) {
            VkQueue rawComputeQ;
            vkGetDeviceQueue(dev->_dev, devInfo.computeQueueFamily.value(), 0, &rawComputeQ);
            dev->_computeQ = new VulkanCommandQueue(dev.get(),
                                                    devInfo.computeQueueFamily.value(),
                                                    rawComputeQ);
        }

        //Init allocator
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        allocatorInfo.physicalDevice = dev->_phyDev.handle;
        allocatorInfo.device = dev->_dev;
        allocatorInfo.instance = dev->_ctx->GetHandle();

        VmaVulkanFunctions fn{};
        fn.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        fn.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
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
        if (dev->_features.supportsGetMemReq2 && dev->_features.supportsDedicatedAlloc) {
            fn.vkGetBufferMemoryRequirements2KHR 
                = (PFN_vkGetBufferMemoryRequirements2KHR)vkGetBufferMemoryRequirements2KHR;
            fn.vkGetImageMemoryRequirements2KHR 
                = (PFN_vkGetImageMemoryRequirements2KHR)vkGetImageMemoryRequirements2KHR;
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
        vkGetPhysicalDeviceProperties(dev->_phyDev.handle, &deviceProps);

        dev->_adpInfo.apiVersion.major = VK_API_VERSION_MAJOR(deviceProps.apiVersion);
        dev->_adpInfo.apiVersion.minor = VK_API_VERSION_MINOR(deviceProps.apiVersion);
        dev->_adpInfo.apiVersion.subminor = 0;
        dev->_adpInfo.apiVersion.patch = VK_API_VERSION_PATCH(deviceProps.apiVersion);

        dev->_adpInfo.driverVersion = deviceProps.driverVersion;
        dev->_adpInfo.vendorID = deviceProps.vendorID;
        dev->_adpInfo.deviceID = deviceProps.deviceID;

        dev->_adpInfo.deviceName = phyDev->name;


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
    
    
    const VkInstance& VulkanDevice::GetInstance() const {return _ctx->GetHandle();}

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
        VkSwapchainKHR deviceSwapchain = vkSC->GetHandle();
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 0;
        presentInfo.pWaitSemaphores = nullptr;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &deviceSwapchain;
        auto imageIndex = vkSC->GetCurrentImageIdx();
        presentInfo.pImageIndices = &imageIndex;

        vkSC->MarkCurrentImageInUse();

        //object presentLock = vkSC.PresentQueueIndex == _graphicsQueueIndex ? _graphicsQueueLock : vkSC;
        //lock(presentLock)
        {
            auto res = vkQueuePresentKHR(_gfxQ->GetHandle(), &presentInfo);
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
        vkDestroySemaphore(_dev->LogicalDev(), _timelineSem, nullptr);
    }
    
    uint64_t VulkanFence::GetSignaledValue() {
        uint64_t value;
        vkGetSemaphoreCounterValue(_dev->LogicalDev(), _timelineSem, &value);

        return value;
    }
    void VulkanFence::SignalFromCPU(uint64_t signalValue) {
        VkSemaphoreSignalInfo signalInfo {};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signalInfo.pNext = nullptr;
        signalInfo.semaphore = _timelineSem;
        signalInfo.value = signalValue;

        vkSignalSemaphoreKHR(_dev->LogicalDev(), &signalInfo);
    }
    bool VulkanFence::WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) {
        VkSemaphoreWaitInfo waitInfo {};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.pNext = nullptr;
        waitInfo.flags = 0;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &_timelineSem;
        waitInfo.pValues = &expectedValue;

        auto res = vkWaitSemaphoresKHR(_dev->LogicalDev(), &waitInfo, timeoutMs * 1000000);

        switch (res) {
        //On success, this command returns
            case VK_SUCCESS: return true;
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
        VK_CHECK(vkCreateSemaphore(dev->LogicalDev(), &createInfo, nullptr, &timelineSemaphore));

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
        VK_CHECK(vkAllocateCommandBuffers(mgr->_dev, &cbufInfo, &cbuf));
        return cbuf;

    }

    void _CmdPoolContainer::FreeBuffer(VkCommandBuffer buf) {
        vkFreeCommandBuffers(mgr->_dev, pool, 1, &buf);
    }
    
    void _CmdPoolMgr::_ReleaseCmdPoolHolder(_CmdPoolContainer* holder) {
        std::scoped_lock _lock{ _m_cmdPool };
        vkResetCommandPool(_dev, holder->pool, 0);
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

                VK_CHECK(vkCreateCommandPool(_dev, &cmdPoolCI, nullptr, &raw_pool));
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

    VulkanCommandQueue::VulkanCommandQueue(
        VulkanDevice* dev,
        std::uint32_t queueFamily,
        VkQueue q
    )
        : _dev(dev)
        , _cmdPoolMgr(dev->LogicalDev(), queueFamily)
        , _q(q)
    { }

    void VulkanCommandQueue::EncodeSignalEvent(IEvent* event, uint64_t value) {
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

        vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE);

    }

    void VulkanCommandQueue::EncodeWaitForEvent(IEvent* evt, uint64_t value) {
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

        vkQueueSubmit(_q, 1, &submitInfo, VK_NULL_HANDLE);
    }

    void VulkanCommandQueue::SubmitCommand(ICommandList* cmd) {
        assert(cmd != nullptr);
        auto* vkCmd = PtrCast<VulkanCommandList>(cmd);
        auto rawCmdBuf = vkCmd->GetHandle();
            
        VkSubmitInfo info {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &rawCmdBuf;
       
        VK_CHECK(vkQueueSubmit(
            _q, 1, &info, nullptr
        ));
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

