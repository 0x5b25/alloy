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
        //Umberlla stages
        All,
        Draw,
        NonPixelShading,
        AllShading,
        //Per stage
        //TopOfPipe,
        //BottomOfPipe,
        INPUT_ASSEMBLER,
        VERTEX_SHADING,
        PIXEL_SHADING,
        DEPTH_STENCIL,
        RENDER_TARGET,
        COMPUTE_SHADING,
        RAYTRACING,
        COPY,
        RESOLVE,
        EXECUTE_INDIRECT,
        EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO,
        BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
        COPY_RAYTRACING_ACCELERATION_STRUCTURE,
        //SPLIT, //used in pairs, indicates sync across commandlists

        ALLOY_BITFLAG_MAX
    };

    enum class ResourceAccess {
        COMMON,
        VERTEX_BUFFER,
        CONSTANT_BUFFER,
        INDEX_BUFFER,
        RENDER_TARGET,
        UNORDERED_ACCESS,
        DEPTH_STENCIL_WRITE,
        DEPTH_STENCIL_READ,
        SHADER_RESOURCE,
        STREAM_OUTPUT,
        INDIRECT_ARGUMENT,
        PREDICATION,
        COPY_DEST,
        COPY_SOURCE,
        RESOLVE_DEST,
        RESOLVE_SOURCE,
        RAYTRACING_ACCELERATION_STRUCTURE_READ,
        RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
        SHADING_RATE_SOURCE,

        ALLOY_BITFLAG_MAX
    };


    enum class TextureLayout {
        COMMON = 0,
        PRESENT,
        COMMON_READ,
        RENDER_TARGET,
        UNORDERED_ACCESS,
        DEPTH_STENCIL_WRITE,
        DEPTH_STENCIL_READ,
        SHADER_RESOURCE,
        COPY_SOURCE,
        COPY_DEST,
        RESOLVE_SOURCE,
        RESOLVE_DEST,
        SHADING_RATE_SOURCE,
        
        UNDEFINED	/*= 0xffffffff*/,
    };


    using PipelineStages = common::BitFlags<PipelineStage>;

    using ResourceAccesses = common::BitFlags<ResourceAccess>;

    struct MemoryBarrierDescription {
        //Sync stages
        PipelineStages stagesBefore;
        PipelineStages stagesAfter;
        //Resource access
        ResourceAccesses accessBefore;
        ResourceAccesses accessAfter;
    };

    struct MemoryBarrierResource {};
    
    struct BufferBarrierResource {
        common::sp<alloy::IBuffer> resource;
    };

    struct TextureBarrierResource {
        TextureLayout fromLayout;
        TextureLayout toLayout;
        common::sp<ITexture> resource;
    };

    struct BarrierDescription {
        MemoryBarrierDescription memBarrier;

        std::variant< MemoryBarrierResource,
                       BufferBarrierResource,
                       TextureBarrierResource> resourceInfo;

    };
    


} // namespace alloy


