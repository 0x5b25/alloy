

#include "alloy/common/Macros.h"
#include "VulkanContext.hpp"

#include <cassert>


//Set platform defines at build time for volk to pick up.
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


#include <volk.h>

// Special include order: we can't leak platform details into headers
#include "VkSurfaceUtil.hpp"


namespace alloy::VK::priv{

SurfaceContainer CreateSurface(alloy::vk::VulkanContext& ctx, const alloy::SwapChainSource* swapchainSource) {
	assert(swapchainSource != nullptr);

	auto hInst = ctx.GetHandle();
	auto& fnTable = ctx.GetFnTable();

	switch (swapchainSource->tag) {
		case alloy::SwapChainSource::Tag::Opaque:{
			auto opaqueSource = (alloy::OpaqueSwapChainSource*)swapchainSource;
			return {(VkSurfaceKHR)opaqueSource->handle, false};
		}

#ifdef VLD_PLATFORM_WIN32
		case alloy::SwapChainSource::Tag::Win32: {
			auto win32Source = (alloy::Win32SwapChainSource*)swapchainSource;
			VkWin32SurfaceCreateInfoKHR surfaceCI{};
			surfaceCI.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCI.pNext = nullptr;
			surfaceCI.flags = 0;
			surfaceCI.hwnd = (HWND)win32Source->hWnd;
			surfaceCI.hinstance = (HINSTANCE)win32Source->hInstance;
			VkSurfaceKHR surface;
			VkResult result = fnTable.vkCreateWin32SurfaceKHR(hInst, &surfaceCI, nullptr, &surface);

			return {surface, true};
		}
#endif


#if defined(VLD_PLATFORM_LINUX)
		case alloy::SwapChainSource::Tag::Wayland: {
			auto wlSource = (alloy::WaylandSwapChainSource*)swapchainSource;
			VkWaylandSurfaceCreateInfoKHR surfaceCI {
				.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR
			};

			surfaceCI.display = (wl_display*)wlSource->display;
			surfaceCI.surface = (wl_surface*)wlSource->surface;
			VkSurfaceKHR surface;
			VkResult result = fnTable.vkCreateWaylandSurfaceKHR(hInst, &surfaceCI, nullptr, &surface);

			return {surface, true};
		}
#endif

		default: {
			return {VK_NULL_HANDLE, false};
		}
	}

}

std::string GetSurfaceExtension(alloy::SwapChainSource* swapchainSource) {
	assert(swapchainSource != nullptr);

	switch (swapchainSource->tag) {
		case alloy::SwapChainSource::Tag::Opaque:
			return "VK_KHR_surface";
		case alloy::SwapChainSource::Tag::Win32:
			return "VK_KHR_win32_surface";
		case alloy::SwapChainSource::Tag::Wayland:
			return "VK_KHR_wayland_surface";
		default: {
			return {};
		}
	}
}

}
