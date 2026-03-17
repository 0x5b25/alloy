#pragma once

#include "Context.hpp"
#include "RenderPass.hpp"
#include "CommandQueue.hpp"
#include "Buffer.hpp"
#include "CommandList.hpp"
#include "ResourceBarrier.hpp"
#include "BindableResource.hpp"
#include "SyncObjects.hpp"
#include "FixedFunctions.hpp"
#include "FrameBuffer.hpp"
#include "GraphicsDevice.hpp"
#include "Helpers.hpp"
#include "Pipeline.hpp"
#include "ResourceFactory.hpp"
#include "Sampler.hpp"
#include "Shader.hpp"
#include "SwapChain.hpp"
#include "SwapChainSources.hpp"
#include "Texture.hpp"
#include "Types.hpp"

/* Coordinate systems: 
 *   Alloy use DX12/Metal convention: lefthand .
 *   Vulkan supports flipping the NDC by passing negative height during SetViewport.
 * 
 * Normalized device coord:
 * DX12/Metal: 
 *                   1.0
 *                    +Y
 *                    ▲   +Z 1.0
 *                    │  /
 *                    │ /
 *                    │/
 *  -1.0 -X ----------+----------► +X 1.0
 *                   /│
 *                  / │
 *            -Z   /  │
 *           -1.0     -Y
 *                   -1.0
 *
 * Vulkan: 
 *                   -1.0
 *                    -Y  +Z 1.0
 *                    │  /
 *                    │ /
 *                    │/
 *  -1.0 -X ----------+----------► +X 1.0
 *                   /│
 *                  / │
 *            -Z   /  │
 *           -1.0     ▼
 *                    +Y
 *                   1.0
 * 
 * Frame buffer coord:
 * DX12/Metal/Vulkan: 
 *   (0,0)
 *     +-----------► +X
 *     │
 *     │
 *     │
 *     ▼ 
 *    +Y
 */