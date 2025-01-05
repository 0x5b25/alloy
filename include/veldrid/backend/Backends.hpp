#pragma once

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SwapChainSources.hpp"

namespace Veldrid
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
    
    sp<GraphicsDevice> CreateDX12GraphicsDevice(
        const GraphicsDevice::Options& options
    );

    sp<GraphicsDevice> CreateVulkanGraphicsDevice(
        const GraphicsDevice::Options& options
    );

    sp<GraphicsDevice> CreateMetalGraphicsDevice(
        const GraphicsDevice::Options& options
    );

    sp<GraphicsDevice> CreateDefaultGraphicsDevice(
        const GraphicsDevice::Options& options
    );

} // namespace Veldrid


