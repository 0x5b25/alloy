#pragma once

#include "common/RefCnt.hpp"
#include "common/BitFlags.hpp"
#include "GraphicsDevice.hpp"
#include "FixedFunctions.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"

#include <variant>

// Pipeline stage Concepts:
//  alloy   Vulkan    DX12    Metal
//

namespace alloy
{
    enum class PipelineStage {
        TopOfPipe,
        BottomOfPipe,
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

        MAX_VALUE
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
        NO_ACCESS,

        MAX_VALUE
    };


    enum class TextureLayout {
        //UNDEFINED	= 0xffffffff,
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
    };


    using PipelineStages = BitFlags<PipelineStage>;

    using ResourceAccesses = BitFlags<ResourceAccess>;

    struct MemoryBarrierDescription {
        ResourceAccesses accessBefore;
        ResourceAccesses accessAfter;
    };

    
    struct BufferBarrierDescription : public MemoryBarrierDescription {
        Veldrid::sp<Veldrid::Buffer> resource;
    };

    struct TextureBarrierDescription : public MemoryBarrierDescription {
        TextureLayout fromLayout;
        TextureLayout toLayout;
        Veldrid::sp<Veldrid::Texture> resource;
    };

    struct BarrierDescription {
        //Sync stages
        PipelineStages stagesBefore;
        PipelineStages stagesAfter;
        std::vector<std::variant< MemoryBarrierDescription,
                       BufferBarrierDescription,
                       TextureBarrierDescription>> barriers;
    };


} // namespace alloy


