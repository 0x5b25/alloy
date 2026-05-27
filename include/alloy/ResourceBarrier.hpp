#pragma once

#include "common/RefCnt.hpp"
#include "common/BitFlags.hpp"
//#include "GraphicsDevice.hpp"
#include "FixedFunctions.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"

#include <variant>

// Pipeline stage Concepts:
//  alloy   Vulkan    DX12    Metal
//

///#TODO: have separation between in-renderpass memory barriers & inter-pass barriers
//According to metal development guide: 
//    https://developer.apple.com/documentation/metal/resource_synchronization
//
//```
//    Your app is responsible for manually synchronizing the resources that Metal
//    doesn’t track. You can synchronize resources with these mechanisms, which are
//    in ascending scope order:
//
//        Memory barriers
//
//        Memory fences
//
//        Metal events
//
//        Metal shared events
//
//    A memory barrier forces any subsequent commands to wait until the previous
//    commands in a pass (such as a render or compute pass) finishes using memory.
//    You can limit the scope of a memory barrier to a buffer, texture, render attachment,
//    or a combination.
//
//    An MTLFence synchronizes access to one or more resources across different passes
//    within a command buffer. Use fences to specify any inter-pass resource dependencies
//    within the same command buffer.
//
//```
//
//For a more concise resoure sync design, consider using vkEvent(vulkan) and
//split barriers(dx12):
//Analysis from webgpu : https://github.com/gpuweb/gpuweb/issues/27
//```
//    Tips for best performance (for AMD):
//
//        combine transitions
//        use the most specific state, but also - combine states
//        give driver time to handle the transition
//            D3D12: split barriers
//            Vulkan: vkCmdSetEvent + vkCmdWaitEvents
//
//    Nitrous engine (Oxide Games, GDC 2017 presentation slide 36) approach:
//
//        engine is auto-tracking the current state, the user requests new state only
//        extended (from D3D12) resource state range that maps to Vulkan barriers
//
//    Overall, in terms of flexibility/configuration,
//    Vulkan barriers >> D3D12 barriers >> Metal. Developers seem to prefer
//    D3D12 style (TODO: confirm with more developers!).
//```

//enum class RenderStage : std::uint32_t {
//    Object,
//    Mesh,
//    Vertex,
//    Fragment,
//    Tile,
//
//    MAX_VALUE
//};

namespace alloy
{
    enum class PipelineStage {
        // Umbrella scopes / coarse aliases.
        AllCommands,
        AllGraphics,
        AllShaders,

        // Common explicit stages.
        DrawIndirect,
        VertexInput,
        VertexShader,
        MeshShader,
        FragmentShader,
        DepthStencil,
        ColorOutput,
        ComputeShader,
        RayTracing,
        Copy,
        BuildAS,

        ALLOY_BITFLAG_MAX,

        // Not a bit flag. Use an empty PipelineStages mask for "no stages".
        //None
    };

    enum class ResourceAccess {
        IndirectArgumentRead,
        VertexBufferRead,
        IndexBufferRead,
        ConstantBufferRead,

        ShaderResourceRead,
        UnorderedAccess,

        RenderTarget,
        DepthStencilRead,
        DepthStencilWrite,

        CopySource,
        CopyDest,

        AccelerationStructureRead,
        AccelerationStructureWrite,

        Present,

        ALLOY_BITFLAG_MAX,

        // Not a bit flag. Use an empty ResourceAccesses mask for "no access".
        //None

    };


    enum class TextureLayout {
        Undefined,
        General,
        ShaderReadOnly,
        Storage,
        ColorAttachment,
        DepthStencilReadOnly,
        DepthStencilWrite,
        CopySource,
        CopyDest,
        ResolveSource,
        ResolveDest,
        Present,
    };


    using PipelineStages = common::BitFlags<PipelineStage>;
    using PipelineStageMask = PipelineStages;

    using ResourceAccesses = common::BitFlags<ResourceAccess>;
    using ResourceAccessMask = ResourceAccesses;

    struct ResourceState {
        PipelineStageMask stages;
        ResourceAccessMask access;
    };

    struct TextureState {
        PipelineStageMask stages;
        ResourceAccessMask access;
        TextureLayout layout;
    };

    struct BufferBarrierOp {
        common::sp<alloy::BufferRange> buffer; //#TODO: Use BufferRange
        ResourceState from;
        ResourceState to;
    };

    struct TextureBarrierOp {
        common::sp<ITextureView> texture;
        TextureState from;
        TextureState to;
    };

    using BarrierOp = std::variant<BufferBarrierOp, TextureBarrierOp>;
    
} // namespace alloy


