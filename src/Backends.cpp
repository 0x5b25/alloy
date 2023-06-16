#include "veldrid/backend/Backends.hpp"
#include "veldrid/common/Macros.h"

namespace Veldrid
{
#ifndef VLD_BACKEND_VK
    sp<GraphicsDevice> CreateVulkanGraphicsDevice(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    ){return nullptr;}
#endif

#ifndef VLD_BACKEND_MTL
    sp<GraphicsDevice> CreateMetalGraphicsDevice(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    ){return nullptr;}
#endif

    sp<GraphicsDevice> CreateDefaultGraphicsDevice(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    ){

#ifdef VLD_PLATFORM_WIN32
        return CreateVulkanGraphicsDevice(options, swapChainSource);
#endif

#if defined(VLD_PLATFORM_MACOS) || defined(VLD_PLATFORM_IOS)
        return CreateMetalGraphicsDevice(options, swapChainSource);
#endif
        
        return nullptr;
        
    }
} // namespace Veldrid
