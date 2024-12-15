#include "VkCommon.hpp"

#include <volk.h>


#include "veldrid/common/Macros.h"


#include <cstdint>



constexpr const char* GetVkPlatformSurfaceExt() {
#ifdef VLD_PLATFORM_WIN32
    return VkInstExtNames::VK_KHR_WIN32_SURFACE;
#elif defined VLD_PLATFORM_ANDROID
	return VkInstExtNames::VK_KHR_ANDROID_SURFACE;
#else
	return "";
#endif
}


const char* VkInstExtNames::VK_KHR_SURFACE = "VK_KHR_surface";
const char* VkInstExtNames::VK_KHR_WIN32_SURFACE = "VK_KHR_win32_surface";
const char* VkInstExtNames::VK_KHR_XCB_SURFACE = "VK_KHR_xcb_surface";
const char* VkInstExtNames::VK_KHR_XLIB_SURFACE = "VK_KHR_xlib_surface";
const char* VkInstExtNames::VK_KHR_WAYLAND_SURFACE = "VK_KHR_wayland_surface";
const char* VkInstExtNames::VK_KHR_ANDROID_SURFACE = "VK_KHR_android_surface";
const char* VkInstExtNames::VK_MVK_MACOS_SURFACE = "VK_MVK_macos_surface";
const char* VkInstExtNames::VK_MVK_IOS_SURFACE = "VK_MVK_ios_surface";
const char* VkInstExtNames::VK_EXT_METAL_SURFACE = "VK_EXT_metal_surface";
const char* VkInstExtNames::VK_EXT_DEBUG_REPORT = "VK_EXT_debug_report";
const char* VkInstExtNames::VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES2 = "VK_KHR_get_physical_device_properties2";
const char* VkInstExtNames::VK_KHR_PORTABILITY_SUBSET = "VK_KHR_portability_subset";

const char* VkDevExtNames::VK_KHR_SWAPCHAIN = "VK_KHR_swapchain";
const char* VkDevExtNames::VK_EXT_DEBUG_MARKER = "VK_EXT_debug_marker";
const char* VkDevExtNames::VK_KHR_MAINTENANCE1 = "VK_KHR_maintenance1";
const char* VkDevExtNames::VK_KHR_GET_MEMORY_REQ2 = "VK_KHR_get_memory_requirements2";
const char* VkDevExtNames::VK_KHR_DEDICATED_ALLOCATION = "VK_KHR_dedicated_allocation";
const char* VkDevExtNames::VK_KHR_DRIVER_PROPS = "VK_KHR_driver_properties";
const char* VkDevExtNames::VK_EXT_DEPTH_CLIP_ENABLE = "VK_EXT_depth_clip_enable";

const char* VkDevExtNames::VK_KHR_TIMELINE_SEMAPHORE =  VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;// "VK_KHR_timeline_semaphore";

const char* VkCommonStrings::StandardValidationLayerName = "VK_LAYER_LUNARG_standard_validation";
const char* VkCommonStrings::KhronosValidationLayerName = "VK_LAYER_KHRONOS_validation";
const char* VkCommonStrings::main = "main";



std::vector<std::string> EnumerateInstanceLayers()
{

    std::uint32_t propCount = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&propCount, nullptr));
    if (propCount == 0) {
        return {};
    }

    std::vector<VkLayerProperties> props(propCount);

    VK_CHECK(vkEnumerateInstanceLayerProperties(&propCount, props.data()));

    std::vector<std::string> ret; ret.reserve(propCount);
    for (int i = 0; i < propCount; i++)
    {
        auto& prop = props[i];
        ret.emplace_back(prop.layerName);
    }

    return ret;
}

std::vector<std::string> EnumerateInstanceExtensions(){
    
    std::uint32_t propCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propCount, nullptr));
    
    if (propCount == 0) {
        return {};
    }

    std::vector<VkExtensionProperties> props(propCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propCount, props.data()));

    std::vector<std::string> ret; ret.reserve(propCount);
    for (int i = 0; i < propCount; i++)
    {
        auto& prop = props[i];
        ret.emplace_back(prop.extensionName);
    }

    return ret;
}

