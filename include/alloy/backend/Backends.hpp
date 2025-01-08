#pragma once

#include "alloy/GraphicsDevice.hpp"
#include "alloy/SwapChainSources.hpp"

namespace alloy
{

    enum class GraphicsBackend{
        /// <summary>
        /// Direct3D 11.
        /// </summary>
        //Direct3D11,
        /// <summary>
        /// Direct3D 12.
        /// </summary>
        Direct3D12,
        /// <summary>
        /// Vulkan.
        /// </summary>
        Vulkan,
        /// <summary>
        /// OpenGL.
        /// </summary>
        //OpenGL,
        /// <summary>
        /// Metal.
        /// </summary>
        Metal,
        /// <summary>
        /// OpenGL ES.
        /// </summary>
        //OpenGLES,
    };    
    
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
