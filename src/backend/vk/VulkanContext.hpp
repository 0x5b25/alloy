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
    class VulkanAdapter;

    struct VulkanDevCaps {
        GraphicsApiVersion apiVersion;
        VkPhysicalDeviceLimits limits;
        VkPhysicalDeviceSparseProperties sparseProperties;

        VkPhysicalDeviceVulkan11Properties properties11;
        VkPhysicalDeviceVulkan12Properties properties12;
        VkPhysicalDeviceVulkan13Properties properties13;
        VkPhysicalDeviceVulkan14Properties properties14;

        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceVulkan11Features features11;
        VkPhysicalDeviceVulkan12Features features12;
        VkPhysicalDeviceVulkan13Features features13;
        VkPhysicalDeviceVulkan14Features features14;

        bool SupportScalarBlockLayout() const { return features12.scalarBlockLayout; }
        bool supportMeshShader;
        bool supportRayTracing;
        bool supportBindless;

        bool isIntegratedGPU;
        bool isResizableBARSupported;

        void ReadFromDevice(VulkanContext& ctx, VkPhysicalDevice adp);
    };

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

        VulkanDevCaps _caps;

        void PopulateAdpMemSegs();
        void PopulateAdpLimits();
        void PopulateAdpFeatures();

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
        VulkanContext& GetCtx() const {return *_ctx.get();}
        const VulkanDevCaps& GetCaps() const { return _caps; }

        const QueueFamilyInfo& GetQueueFamilyInfo() const {return _qFamily;}
    };

    
    class VulkanContext : public IContext{

    public:
        union Capabilities
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

                bool hasDebugUtilExt : 1;

            };
            std::uint32_t value;
        };
    
    private:
        //Helper class to init/deinit volk library
        class VolkCtx {
            static uint32_t _refCnt;
            static std::mutex _m;
        public:
            static bool Ref() {
                std::scoped_lock lock {_m};
                if(_refCnt == 0) {
                    auto res = volkInitialize();
                    if(res == VK_SUCCESS) {
                        _refCnt++;
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
                    volkFinalize();
                }
                _refCnt--;
            }
        };

    private:

        VkInstance _instance;

        Capabilities _caps;
        
        VolkInstanceTable _fnTable;

        VulkanContext(VkInstance instance):
            _instance(instance)
        { }

        //#TODO: Figure out proper way to do handle creation when
        //multiple context exists
        VkDebugReportCallbackEXT _debugCallbackHandle;
        
        static VkBool32 _DbgCbStatic(
            VkDebugReportFlagsEXT                       msgFlags,
            VkDebugReportObjectTypeEXT                  objectType,
            uint64_t                                    object,
            size_t                                      location,
            int32_t                                     msgCode,
            const char* pLayerPrefix,
            const char* pMsg,
            void* pUserData);

        VkBool32 _DbgCb(
            VkDebugReportFlagsEXT                       msgFlags,
            VkDebugReportObjectTypeEXT                  objectType,
            uint64_t                                    object,
            size_t                                      location,
            int32_t                                     msgCode,
            const char* pLayerPrefix,
            const char* pMsg);

    public:

        virtual ~VulkanContext() override {

            if (_debugCallbackHandle != VK_NULL_HANDLE) {
                _fnTable.vkDestroyDebugReportCallbackEXT(_instance, _debugCallbackHandle, nullptr);
            }
            _fnTable.vkDestroyInstance(_instance, nullptr);
            DEBUGCODE(_instance = nullptr);
            VolkCtx::Release();
        }

        static common::sp<VulkanContext> Make(const IContext::Options& opts);

        VkInstance GetHandle() const { return _instance; }
        const VolkInstanceTable& GetFnTable() const { return _fnTable; }

        const Capabilities& GetCaps() const { return _caps; }

        virtual common::sp<IGraphicsDevice> CreateDefaultDevice(
            const IGraphicsDevice::Options& options) override;
        virtual std::vector<common::sp<IPhysicalAdapter>> EnumerateAdapters() override;
    };


} // namespace alloy::vk
