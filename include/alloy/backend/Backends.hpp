#pragma once

#include "alloy/GraphicsDevice.hpp"
#include "alloy/SwapChainSources.hpp"

namespace alloy
{

    
    
    common::sp<IGraphicsDevice> CreateDX12GraphicsDevice(
        const IGraphicsDevice::Options& options
    );

    common::sp<IGraphicsDevice> CreateVulkanGraphicsDevice(
        const IGraphicsDevice::Options& options
    );

    common::sp<IGraphicsDevice> CreateMetalGraphicsDevice(
        const IGraphicsDevice::Options& options
    );

    common::sp<IGraphicsDevice> CreateDefaultGraphicsDevice(
        const IGraphicsDevice::Options& options
    );

} // namespace alloy
