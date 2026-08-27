#pragma once

#include "alloy/common/Macros.h"

#include "alloy/SwapChainSources.hpp"

#include <string>

namespace alloy::vk{
	class VulkanContext;
}

namespace alloy::VK::priv{
	struct SurfaceContainer{
		VkSurfaceKHR surface;
		bool isOwnSurface;//Is this surface managed by us
	};

	SurfaceContainer CreateSurface(alloy::vk::VulkanContext& ctx, const alloy::SwapChainSource* swapchainSource);

	std::string GetSurfaceExtension(alloy::SwapChainSource* swapchainSource);
}
