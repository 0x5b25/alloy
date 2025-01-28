#include "alloy/backend/Backends.hpp"
#include "alloy/common/Macros.h"

namespace alloy
{

#ifndef VLD_BACKEND_DXC
    common::sp<IGraphicsDevice> CreateDX12GraphicsDevice(
        const IGraphicsDevice::Options& options
    ){
        assert(false);
        return nullptr;
    }
#endif

#ifndef VLD_BACKEND_VK
    common::sp<IGraphicsDevice> CreateVulkanGraphicsDevice(
        const IGraphicsDevice::Options& options
    ){
        assert(false);
        return nullptr;
    }
#endif

#ifndef VLD_BACKEND_MTL
    common::sp<IGraphicsDevice> CreateMetalGraphicsDevice(
        const IGraphicsDevice::Options& options
    ){
        assert(false);
        return nullptr;
    }
#endif

    common::sp<IGraphicsDevice> CreateDefaultGraphicsDevice(
        const IGraphicsDevice::Options& options
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
