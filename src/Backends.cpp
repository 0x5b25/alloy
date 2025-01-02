#include "veldrid/backend/Backends.hpp"
#include "veldrid/common/Macros.h"

namespace Veldrid
{

#ifndef VLD_BACKEND_DXC
    sp<GraphicsDevice> CreateDX12GraphicsDevice(
        const GraphicsDevice::Options& options
    ){return nullptr;}
#endif

#ifndef VLD_BACKEND_VK
    sp<GraphicsDevice> CreateVulkanGraphicsDevice(
        const GraphicsDevice::Options& options
    ){return nullptr;}
#endif

#ifndef VLD_BACKEND_MTL
    sp<GraphicsDevice> CreateMetalGraphicsDevice(
        const GraphicsDevice::Options& options
    ){return nullptr;}
#endif

    sp<GraphicsDevice> CreateDefaultGraphicsDevice(
        const GraphicsDevice::Options& options
    ){

#if defined(VLD_PLATFORM_WIN32)
        return CreateDX12GraphicsDevice(options);
#elif defined(VLD_PLATFORM_MACOS) || defined(VLD_PLATFORM_IOS)
        return CreateMetalGraphicsDevice(options);
#else
        return CreateVulkanGraphicsDevice(options);
#endif
        
        return nullptr;
        
    }
} // namespace Veldrid
