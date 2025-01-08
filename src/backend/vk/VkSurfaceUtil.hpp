#pragma once

#include "alloy/common/Macros.h"


/* Set platform defines at build time for volk to pick up.
#if defined(VLD_PLATFORM_WIN32)
	#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(VLD_PLATFORM_ANDROID)
	#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(VLD_PLATFORM_LINUX)
	#define VK_USE_PLATFORM_XLIB_KHR
	#define VK_USE_PLATFORM_WAYLAND_KHR
#elif defined(__APPLE__)
	#define VK_USE_PLATFORM_MACOS_MVK
#else
#   error "Platform not supported by this example."
#endif
 */
#include <volk.h>

#include "alloy/SwapChainSources.hpp"

#include <string>

namespace alloy::VK::priv{
	struct SurfaceContainer{
		VkSurfaceKHR surface;
		bool isOwnSurface;//Is this surface managed by us
	};

	SurfaceContainer CreateSurface(VkInstance instance, alloy::SwapChainSource* swapchainSource);

	std::string GetSurfaceExtension(alloy::SwapChainSource* swapchainSource);
}
