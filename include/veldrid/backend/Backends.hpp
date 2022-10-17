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
    
    sp<GraphicsDevice> CreateVulkanGraphicsDevice(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    );

} // namespace Veldrid


