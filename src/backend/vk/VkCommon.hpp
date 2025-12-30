#pragma once


#include "alloy/common/Macros.h"
#include <vector>
#include <string>

/// @brief Helper macro to test the result of Vulkan calls which can return an error.
#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			/*LOGE("Detected Vulkan error: {}", vkb::to_string(err));*/ \
			abort();                                                \
		}                                                           \
	} while (0)

#define VK_ASSERT(expr)                                             \
	do                                                              \
	{                                                               \
		if (!expr)                                                    \
		{                                                           \
			/*LOGE("Detected Vulkan error: {}", vkb::to_string(err));*/ \
			abort();                                                \
		}                                                           \
	} while (0)


struct VkInstExtNames {
    

    constexpr static const char* VK_KHR_SURFACE = "VK_KHR_surface";
    constexpr static const char* VK_KHR_WIN32_SURFACE = "VK_KHR_win32_surface";
    constexpr static const char* VK_KHR_XCB_SURFACE = "VK_KHR_xcb_surface";
    constexpr static const char* VK_KHR_XLIB_SURFACE = "VK_KHR_xlib_surface";
    constexpr static const char* VK_KHR_WAYLAND_SURFACE = "VK_KHR_wayland_surface";
    constexpr static const char* VK_KHR_ANDROID_SURFACE = "VK_KHR_android_surface";
    constexpr static const char* VK_MVK_MACOS_SURFACE = "VK_MVK_macos_surface";
    constexpr static const char* VK_MVK_IOS_SURFACE = "VK_MVK_ios_surface";
    constexpr static const char* VK_EXT_METAL_SURFACE = "VK_EXT_metal_surface";
    constexpr static const char* VK_EXT_DEBUG_REPORT = "VK_EXT_debug_report";
    constexpr static const char* VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES2 = "VK_KHR_get_physical_device_properties2";
    constexpr static const char* VK_KHR_PORTABILITY_SUBSET = "VK_KHR_portability_subset";
    //static const char* VK_KHR_SWAPCHAIN;
};

struct VkDevExtNames {
    //static const char* VK_KHR_SWAPCHAIN;
    //static const char* VK_EXT_DEBUG_MARKER;
    //static const char* VK_KHR_MAINTENANCE1;
    //static const char* VK_KHR_GET_MEMORY_REQ2;
    //static const char* VK_KHR_DEDICATED_ALLOCATION;
    //static const char* VK_KHR_DRIVER_PROPS;
    //static const char* VK_EXT_DEPTH_CLIP_ENABLE;
    //static const char* VK_KHR_SYNCHRONIZATION2;
    //static const char* VK_KHR_TIMELINE_SEMAPHORE;
    //static const char* VK_KHR_DYNAMIC_RENDERING;

    constexpr static auto VK_KHR_SWAPCHAIN = "VK_KHR_swapchain";
    constexpr static auto VK_EXT_DEBUG_MARKER = "VK_EXT_debug_marker";
    constexpr static auto VK_KHR_MAINTENANCE1 = "VK_KHR_maintenance1";
    constexpr static auto VK_KHR_GET_MEMORY_REQ2 = "VK_KHR_get_memory_requirements2";
    constexpr static auto VK_KHR_DEDICATED_ALLOCATION = "VK_KHR_dedicated_allocation";
    constexpr static auto VK_KHR_DRIVER_PROPS = "VK_KHR_driver_properties";
    constexpr static auto VK_EXT_DEPTH_CLIP_ENABLE = "VK_EXT_depth_clip_enable";

    constexpr static auto VK_KHR_SYNCHRONIZATION2 =  "VK_KHR_synchronization2";// "VK_KHR_timeline_semaphore";
    constexpr static auto VK_KHR_TIMELINE_SEMAPHORE =  "VK_KHR_timeline_semaphore";// "VK_KHR_timeline_semaphore";
    constexpr static auto VK_KHR_DYNAMIC_RENDERING = "VK_KHR_dynamic_rendering";

};

struct VkCommonStrings {    
    constexpr static auto StandardValidationLayerName = "VK_LAYER_LUNARG_standard_validation";
    constexpr static auto KhronosValidationLayerName = "VK_LAYER_KHRONOS_validation";
    constexpr static auto main = "main";
};




inline constexpr const char* GetVkPlatformSurfaceExt() {
#ifdef VLD_PLATFORM_WIN32
    return VkInstExtNames::VK_KHR_WIN32_SURFACE;
#elif defined VLD_PLATFORM_ANDROID
	return VkInstExtNames::VK_KHR_ANDROID_SURFACE;
#else
	return "";
#endif
}

std::vector<std::string> EnumerateInstanceLayers();
std::vector<std::string> EnumerateInstanceExtensions();
