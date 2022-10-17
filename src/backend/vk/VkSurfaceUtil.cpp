#include "VkSurfaceUtil.hpp"

#include "veldrid/common/Macros.h"

#include <cassert>


VkSurfaceKHR CreateSurface(VkInstance instance, Veldrid::SwapChainSource* swapchainSource) {
	assert(swapchainSource != nullptr);

	switch (swapchainSource->tag) {
		case Veldrid::SwapChainSource::Tag::Opaque:{
			auto opaqueSource = (Veldrid::OpaqueSwapchainSource*)swapchainSource;
			return (VkSurfaceKHR)opaqueSource->handle;
		}
		case Veldrid::SwapChainSource::Tag::Win32: {
			auto win32Source = (Veldrid::Win32SwapchainSource*)swapchainSource;
			VkWin32SurfaceCreateInfoKHR surfaceCI{};
			surfaceCI.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCI.pNext = nullptr;
			surfaceCI.flags = 0;
			surfaceCI.hwnd = (HWND)win32Source->hWnd;
			surfaceCI.hinstance = (HINSTANCE)win32Source->hInstance;
			VkSurfaceKHR surface;
			VkResult result = vkCreateWin32SurfaceKHR(instance, &surfaceCI, nullptr, &surface);
			
			return surface;
		}
		default: {
			return VK_NULL_HANDLE;
		}
	}

}

std::string GetSurfaceExtension(Veldrid::SwapChainSource* swapchainSource) {
	assert(swapchainSource != nullptr);

	switch (swapchainSource->tag) {
		case Veldrid::SwapChainSource::Tag::Opaque: 
			return "VK_KHR_surface";
		case Veldrid::SwapChainSource::Tag::Win32:
			return "VK_KHR_win32_surface";
		default: {
			return {};
		}
	}
}
