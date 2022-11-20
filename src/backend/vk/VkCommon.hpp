#pragma once


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

constexpr const char* GetVkPlatformSurfaceExt();



struct VkInstExtNames {
    static const char* VK_KHR_SURFACE;
    static const char* VK_KHR_WIN32_SURFACE;
    static const char* VK_KHR_XCB_SURFACE;
    static const char* VK_KHR_XLIB_SURFACE;
    static const char* VK_KHR_WAYLAND_SURFACE;
    static const char* VK_KHR_ANDROID_SURFACE;
    //static const char* VK_KHR_SWAPCHAIN;
    static const char* VK_MVK_MACOS_SURFACE;
    static const char* VK_MVK_IOS_SURFACE;
    static const char* VK_EXT_METAL_SURFACE;
    static const char* VK_EXT_DEBUG_REPORT;
    //static const char* VK_EXT_DEBUG_MARKER;
    static const char* VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES2;
    static const char* VK_KHR_PORTABILITY_SUBSET;
};

struct VkDevExtNames {
    static const char* VK_KHR_SWAPCHAIN;
    static const char* VK_EXT_DEBUG_MARKER;
    static const char* VK_KHR_MAINTENANCE1;
    static const char* VK_KHR_GET_MEMORY_REQ2;
    static const char* VK_KHR_DEDICATED_ALLOCATION;
    static const char* VK_KHR_DRIVER_PROPS;

};

struct VkCommonStrings {
    static const char* StandardValidationLayerName;
    static const char* KhronosValidationLayerName;
    static const char* main;
};

std::vector<std::string> EnumerateInstanceLayers();
std::vector<std::string> EnumerateInstanceExtensions();
