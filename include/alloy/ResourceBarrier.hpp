#pragma once

#include "common/RefCnt.hpp"
#include "common/BitFlags.hpp"
//#include "GraphicsDevice.hpp"
#include "FixedFunctions.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"

#include <variant>
#include <optional>

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
        DepthStencil,
        DepthStencilReadOnly,

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
        DepthStencil,
        DepthStencilReadOnly,
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

    class ICommandQueue;

    // Optional cross-queue ownership transfer attached to a barrier op.
    //
    // Vulkan:
    //   A transfer is two-sided: record the same barrier both on srcQueue's
    //   command list (the release) and on dstQueue's (the acquire).
    //
    //   The backend decides which half to emit by comparing the recording
    //   command list's queue against src/dst:
    //     - recording on srcQueue  -> release half
    //     - recording on dstQueue  -> acquire half
    //     - srcQueue and dstQueue resolve to the same family/type -> no 
    //        transfer is needed and a plain barrier is emitted 
    //        (the field is ignored).
    // DX12:
    //   No acqiure and release is needed, but resources must be in COMMON
    //   state before hand off. Buffers are auto-decay'd on command buffer 
    //   end. Textures can't auto-decay unless in read only states. So we
    //   always transition textures to COMMON on release half.
    //
    // Each half carry on of the from/to states; for textures both carry the
    // same layout transition (which happens once, between the two halves).
    // For example: shader write (gfxQ) -> copy src (xferQ)
    //   release: from.stage = ComputeShader
    //            from.access = UnorderedAccess
    //            from.layout = Storage
    //            to.stage = None
    //            to.access = None
    //            to.layout = CopySource
    //
    //   acquire: from.stage = None
    //            from.access = None
    //            from.layout = Storage
    //            to.stage = Copy
    //            to.access = CopySource
    //            to.layout = CopySource
    //
    // Note: barriers alone won't sync between queues, use a queue-to-queue 
    // IEvent signal/wait in between.
    struct QueueTransfer {
        ICommandQueue* srcQueue; // releasing queue
        ICommandQueue* dstQueue; // acquiring queue
    };

    struct BufferBarrierOp {
        common::sp<alloy::BufferRange> buffer; //#TODO: Use BufferRange
        ResourceState from;
        ResourceState to;
        // nullopt == same-queue barrier (default, legacy behavior).
        std::optional<QueueTransfer> queueTransfer;
    };

    struct TextureBarrierOp {
        common::sp<ITextureView> texture;
        TextureState from;
        TextureState to;
        // nullopt == same-queue barrier (default, legacy behavior).
        std::optional<QueueTransfer> queueTransfer;
    };

    using BarrierOp = std::variant<BufferBarrierOp, TextureBarrierOp>;
    
} // namespace alloy


