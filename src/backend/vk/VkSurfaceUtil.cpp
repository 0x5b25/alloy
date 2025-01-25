#include "VkSurfaceUtil.hpp"

#include "alloy/common/Macros.h"

#include <cassert>


namespace alloy::VK::priv{

SurfaceContainer CreateSurface(VkInstance instance, alloy::SwapChainSource* swapchainSource) {
	assert(swapchainSource != nullptr);

	switch (swapchainSource->tag) {
		case alloy::SwapChainSource::Tag::Opaque:{
			auto opaqueSource = (alloy::OpaqueSwapChainSource*)swapchainSource;
			return {(VkSurfaceKHR)opaqueSource->handle, false};
		}
		case alloy::SwapChainSource::Tag::Win32: {
			auto win32Source = (alloy::Win32SwapChainSource*)swapchainSource;
			VkWin32SurfaceCreateInfoKHR surfaceCI{};
			surfaceCI.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCI.pNext = nullptr;
			surfaceCI.flags = 0;
			surfaceCI.hwnd = (HWND)win32Source->hWnd;
			surfaceCI.hinstance = (HINSTANCE)win32Source->hInstance;
			VkSurfaceKHR surface;
			VkResult result = vkCreateWin32SurfaceKHR(instance, &surfaceCI, nullptr, &surface);
			
			return {surface, true};
		}
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
		default: {
			return {};
		}
	}
}

}
