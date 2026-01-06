#pragma once

#include <volk.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>
#include <format>
#include <iostream>

#include "alloy/Context.hpp"

#include "VkCommon.hpp"

namespace alloy::vk
{
    class VulkanContext;

    class VulkanAdapter : public IPhysicalAdapter {

    public:
        struct QueueFamilyInfo {
            
            uint32_t graphicsQueueFamily; 
            bool graphicsQueueSupportsCompute;
            std::optional<uint32_t> computeQueueFamily;
            std::optional<uint32_t> transferQueueFamily;
        };

    private:

        common::sp<VulkanContext> _ctx;
        QueueFamilyInfo _qFamily;

        VkPhysicalDevice _handle;

        void PopulateAdpLimits();
        void PopulateAdpMemSegs();

    public:
        static common::sp<VulkanAdapter> Make(
            const common::sp<VulkanContext>& ctx,
            VkPhysicalDevice handle
        );

        VulkanAdapter(
            const common::sp<VulkanContext>& ctx,
            VkPhysicalDevice handle
        );

        virtual common::sp<IGraphicsDevice> RequestDevice(
            const IGraphicsDevice::Options& options) override;
    
        VkPhysicalDevice GetHandle() const {return _handle;}
        VulkanContext* GetCtx() const {return _ctx.get();}

        const QueueFamilyInfo& GetQueueFamilyInfo() const {return _qFamily;}
    };

    
    class VulkanContext : public IContext{

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

        class VolkCtx {

            static uint32_t _refCnt;
            static std::mutex _m;
            
            static Features _features;

            static VkDebugReportCallbackEXT _debugCallbackHandle;
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

            
            static void InitContext() {
                auto _ToSet = [](auto&& obj) {
                    return std::unordered_set(obj.begin(), obj.end());
                };

                std::unordered_set<std::string> availableInstanceLayers{ _ToSet(EnumerateInstanceLayers()) };
                std::unordered_set<std::string> availableInstanceExtensions{ _ToSet(EnumerateInstanceExtensions()) };

                uint32_t vkAPIVer = 0;
                vkEnumerateInstanceVersion(&vkAPIVer);

                VkApplicationInfo appInfo{};
                appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                appInfo.pApplicationName = "";
                appInfo.applicationVersion = 0;
                appInfo.pEngineName = "alloy";
                appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
                appInfo.apiVersion = VK_API_VERSION_1_0;

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

                VkInstance instance;
                VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
                volkLoadInstanceOnly(instance);

                
                if (_features.hasDebugReportExt) {
                    //Debug.WriteLine("Enabling Vulkan Debug callbacks.");
                    VkDebugReportCallbackCreateInfoEXT debugCallbackCI{};
                    debugCallbackCI.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
                    debugCallbackCI.pNext = nullptr;
                    debugCallbackCI.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
                    debugCallbackCI.pfnCallback = &_DbgCb;

                    VK_CHECK(vkCreateDebugReportCallbackEXT(
                        instance, &debugCallbackCI, nullptr, &_debugCallbackHandle));       
                }
            }

            
            static void DestroyContext() {
                auto instance = volkGetLoadedInstance();
                if (_debugCallbackHandle != VK_NULL_HANDLE) {
                    vkDestroyDebugReportCallbackEXT(instance, _debugCallbackHandle, nullptr);
                }

                vkDestroyInstance(instance, nullptr);
            }

        public:
            static bool Ref() {
                std::scoped_lock lock {_m};
                if(_refCnt == 0) {
                    auto res = volkInitialize();
                    if(res == VK_SUCCESS) {
                        _refCnt++;
                        InitContext();
                        return true;
                    }
                    else { return false; }
                } else {
                    _refCnt++;
                    return true;
                }
            }

            static void Release() {
                std::scoped_lock lock {_m};
                if(_refCnt == 0) return;
                if(_refCnt == 1) {
                    DestroyContext();
                    volkFinalize();
                }
                _refCnt--;
            }

            static const Features& GetFeatures() { return _features; }
        };

    private:

        VkInstance _instance;
        alloy::GraphicsApiVersion _ver;

        VulkanContext():
            _instance(VK_NULL_HANDLE)
        {
            _instance = volkGetLoadedInstance();

            auto version = volkGetInstanceVersion();
            _ver.major = VK_VERSION_MAJOR(version);
            _ver.minor = VK_VERSION_MINOR(version);
            _ver.patch = VK_VERSION_PATCH(version);
        }

    public:

        virtual ~VulkanContext() override {
            DEBUGCODE(_instance = nullptr);
            VolkCtx::Release();            
        }

        static common::sp<VulkanContext> Make(const IContext::Options& opts) {
            assert(false);//figure out a way to set debug extensions
            if(!VolkCtx::Ref()) return nullptr;
            return common::sp{ new VulkanContext() };
        }

        const VkInstance& GetHandle() const { return _instance; }

        const Features& GetFeatures() const { return VolkCtx::GetFeatures(); }
        const alloy::GraphicsApiVersion& GetApiVer() const { return _ver; }

        virtual common::sp<IGraphicsDevice> CreateDefaultDevice(
            const IGraphicsDevice::Options& options) override;
        virtual std::vector<common::sp<IPhysicalAdapter>> EnumerateAdapters() override;
    };


} // namespace alloy::vk
