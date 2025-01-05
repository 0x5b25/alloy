#include "VulkanResourceBarrier.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"
#include "VulkanCommandList.hpp"
#include "Veldrid/common/Common.hpp"

namespace Veldrid
{

    

static VkPipelineStageFlags vk_stage_flags_from_d3d12_barrier(
    const VulkanCommandList* list,
    alloy::PipelineStages sync, 
    const alloy::ResourceAccesses& access)
{
    VkPipelineStageFlags stages = 0;
    static_assert(VK_PIPELINE_STAGE_NONE_KHR == 0);

    if(!sync)
        return VK_PIPELINE_STAGE_NONE_KHR;

    /* Resolve umbrella scopes. */
    /* https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#umbrella-synchronization-scopes */
    if (sync & alloy::PipelineStage::All)
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    /* Split barriers are currently broken in the D3D12 runtime, so they cannot be used,
     * but the spec for them is rather unfortunate, you're meant to synchronize once with
     * SyncAfter = SPLIT, and then SyncBefore = SPLIT to complete the barrier.
     * Apparently, there can only be one SPLIT barrier in flight for each (sub-)resource
     * which is extremely weird and suggests we have to track a VkEvent to make this work,
     * which is complete bogus. SPLIT barriers are allowed cross submissions even ...
     * Only reasonable solution is to force ALL_COMMANDS_BIT when SPLIT is observed. */
    //if (sync == D3D12_BARRIER_SYNC_SPLIT)
    //    return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    if (sync & alloy::PipelineStage::Draw)
    {
        sync |= alloy::PipelineStage::INPUT_ASSEMBLER | //Orinially D3D12_BARRIER_SYNC_INDEX_INPUT
                alloy::PipelineStage::VERTEX_SHADING |
                alloy::PipelineStage::PIXEL_SHADING |
                alloy::PipelineStage::DEPTH_STENCIL |
                alloy::PipelineStage::RENDER_TARGET;
    }

    if (sync & alloy::PipelineStage::AllShading)
    {
        sync |= alloy::PipelineStage::NonPixelShading |
                alloy::PipelineStage::PIXEL_SHADING;
    }

    if (sync & alloy::PipelineStage::NonPixelShading)
    {
        sync |= alloy::PipelineStage::VERTEX_SHADING |
                alloy::PipelineStage::COMPUTE_SHADING;

        ///* Ray tracing is not included in this list in docs,
        // * but the example code for legacy UAV barrier mapping
        // * implies that it should be included. */
        //if ((list->vk_queue_flags & VK_QUEUE_COMPUTE_BIT) &&
        //        d3d12_device_supports_ray_tracing_tier_1_0(list->device))
        //    sync |= D3D12_BARRIER_SYNC_RAYTRACING;
    }

    if (sync & alloy::PipelineStage::INPUT_ASSEMBLER)//Orinially D3D12_BARRIER_SYNC_INDEX_INPUT
        stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;//VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;

    if (sync & alloy::PipelineStage::VERTEX_SHADING)
    {
        //stages |= VK_PIPELINE_STAGE_VERTEX_ATTRIBUTE_INPUT_BIT |
        //        VK_PIPELINE_STAGE_PRE_RASTERIZATION_SHADERS_BIT;
        stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (access == alloy::ResourceAccess::COMMON /*|| (access & alloy::ResourceAccess::STREAM_OUTPUT)*/)
            stages |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }

    if (sync & alloy::PipelineStage::PIXEL_SHADING)
    {
        stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        /* Only add special pipeline stages when required by access masks. */
        if (access == alloy::ResourceAccess::COMMON || (access & alloy::ResourceAccess::SHADING_RATE_SOURCE))
            stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
    }

    if (sync & alloy::PipelineStage::DEPTH_STENCIL)
        stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    if (sync & alloy::PipelineStage::RENDER_TARGET)
        stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (sync & (alloy::PipelineStage::COMPUTE_SHADING /*| D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW*/))
        stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    if (sync & alloy::PipelineStage::RAYTRACING)
        stages |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

    if (sync & alloy::PipelineStage::COPY)
        stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;//VK_PIPELINE_STAGE_COPY_BIT;

    if (sync & alloy::PipelineStage::RESOLVE)
    {
        stages |= VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                /*VK_PIPELINE_STAGE_RESOLVE_BIT |*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    if (sync & alloy::PipelineStage::EXECUTE_INDIRECT) /* PREDICATION is alias for EXECUTE_INDIRECT */
    {
        stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        ///* We might use explicit preprocess. */
        //if (list->device->device_info.device_generated_commands_features_ext.deviceGeneratedCommands ||
        //        list->device->device_info.device_generated_commands_features_nv.deviceGeneratedCommands)
        //{
        //    stages |= VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT;
        //}
    }

    if (sync & alloy::PipelineStage::COPY_RAYTRACING_ACCELERATION_STRUCTURE)
        stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;//VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;
    if (sync & alloy::PipelineStage::BUILD_RAYTRACING_ACCELERATION_STRUCTURE)
        stages |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    if (sync & alloy::PipelineStage::EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO)
    {
        /* Somewhat awkward, but in legacy barriers we have to consider that UNORDERED_ACCESS is used to handle
         * postinfo barrier. See vkd3d_acceleration_structure_end_barrier which broadcasts the barrier to ALL_COMMANDS
         * as a workaround.
         * Application will use UNORDERED_ACCESS access flag, which means we cannot use STAGE_COPY_BIT here. */
        stages |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    return stages;
}

static VkAccessFlags vk_access_flags_from_d3d12_barrier(
    const VulkanCommandList* list,
    const alloy::ResourceAccesses& access)
{
    VkAccessFlags2 vk_access = 0;

    if (!access)
        return VK_ACCESS_NONE;
    if (access == alloy::ResourceAccess::COMMON)
        return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

    if (access & alloy::ResourceAccess::VERTEX_BUFFER)
        vk_access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (access & alloy::ResourceAccess::CONSTANT_BUFFER)
        vk_access |= VK_ACCESS_UNIFORM_READ_BIT;
    if (access & alloy::ResourceAccess::INDEX_BUFFER)
        vk_access |= VK_ACCESS_INDEX_READ_BIT;
    if (access & alloy::ResourceAccess::RENDER_TARGET)
        vk_access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    if (access & alloy::ResourceAccess::UNORDERED_ACCESS)
        vk_access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    if (access & alloy::ResourceAccess::DEPTH_STENCIL_READ)
        vk_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if (access & alloy::ResourceAccess::DEPTH_STENCIL_WRITE)
        vk_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if (access & alloy::ResourceAccess::SHADER_RESOURCE)
        vk_access |= VK_ACCESS_SHADER_READ_BIT;
    if (access & alloy::ResourceAccess::STREAM_OUTPUT)
    {
        vk_access |= VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |
                VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
                VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
    }
    if (access & alloy::ResourceAccess::INDIRECT_ARGUMENT)
    {
        /* Add SHADER_READ_BIT here since we might read the indirect buffer in compute for
         * patching reasons. */
        vk_access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

        ///* We might use preprocessing. */
        //if (list->device->device_info.device_generated_commands_features_ext.deviceGeneratedCommands ||
        //        list->device->device_info.device_generated_commands_features_nv.deviceGeneratedCommands)
        //    vk_access |= VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_EXT;
    }

    if (access & alloy::ResourceAccess::COPY_DEST)
        vk_access |= VK_ACCESS_TRANSFER_WRITE_BIT;
    if (access & alloy::ResourceAccess::COPY_SOURCE)
        vk_access |= VK_ACCESS_TRANSFER_READ_BIT;
    if (access & alloy::ResourceAccess::RESOLVE_DEST)
        vk_access |= VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    if (access & alloy::ResourceAccess::RESOLVE_SOURCE)
        vk_access |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    if (access & alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_READ)
        vk_access |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    if (access & alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_WRITE)
        vk_access |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    if (access & alloy::ResourceAccess::SHADING_RATE_SOURCE)
        vk_access |= VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;

    return vk_access;
}

#if 0



    VkPipelineStageFlags _AlToVkStageFlags(const alloy::PipelineStages& stages) {
        VkPipelineStageFlags flags {};
        if(stages[alloy::PipelineStage::TopOfPipe])
            flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if(stages[alloy::PipelineStage::BottomOfPipe])
            flags |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if(stages[alloy::PipelineStage::INPUT_ASSEMBLER])
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        if(stages[alloy::PipelineStage::VERTEX_SHADING])
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if(stages[alloy::PipelineStage::PIXEL_SHADING])
            flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                  | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        if(   stages[alloy::PipelineStage::DEPTH_STENCIL]
           || stages[alloy::PipelineStage::RENDER_TARGET])
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if(stages[alloy::PipelineStage::COMPUTE_SHADING])
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        if(stages[alloy::PipelineStage::RAYTRACING])
            flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        if(stages[alloy::PipelineStage::COPY])
            flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        if(stages[alloy::PipelineStage::RESOLVE])
            flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;//#TODO: reconfirmation needed
        if(stages[alloy::PipelineStage::EXECUTE_INDIRECT])
            flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        //if(stages[alloy::PipelineStage::EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO]){  }
        //if(stages[alloy::PipelineStage::BUILD_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        //if(stages[alloy::PipelineStage::COPY_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        return flags;
    }

    VkAccessFlags _GetAccessFlags(const alloy::ResourceAccesses& accesses) {
        VkAccessFlags flags {};

        if(accesses[alloy::ResourceAccess::COMMON])
            flags |= VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        if(accesses[alloy::ResourceAccess::VERTEX_BUFFER])
            flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        if(accesses[alloy::ResourceAccess::CONSTANT_BUFFER])
            flags |= VK_ACCESS_UNIFORM_READ_BIT;
        if(accesses[alloy::ResourceAccess::INDEX_BUFFER])
            flags |= VK_ACCESS_INDEX_READ_BIT;
        if(accesses[alloy::ResourceAccess::RENDER_TARGET])
            flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if(accesses[alloy::ResourceAccess::UNORDERED_ACCESS])
            flags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        if(accesses[alloy::ResourceAccess::DEPTH_STENCIL_WRITE])
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if(accesses[alloy::ResourceAccess::DEPTH_STENCIL_READ])
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if(accesses[alloy::ResourceAccess::SHADER_RESOURCE])
            flags |= VK_ACCESS_SHADER_READ_BIT;
        if(accesses[alloy::ResourceAccess::STREAM_OUTPUT])
            flags |= VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
        if(accesses[alloy::ResourceAccess::INDIRECT_ARGUMENT])
            flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        if(accesses[alloy::ResourceAccess::PREDICATION])
            flags |= VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT;
        if(accesses[alloy::ResourceAccess::COPY_DEST])
            flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        if(accesses[alloy::ResourceAccess::COPY_SOURCE])
            flags |= VK_ACCESS_TRANSFER_READ_BIT;
        if(accesses[alloy::ResourceAccess::RESOLVE_DEST])
            flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        if(accesses[alloy::ResourceAccess::RESOLVE_SOURCE])
            flags |= VK_ACCESS_TRANSFER_READ_BIT;
        if(accesses[alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_READ])
            flags |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        if(accesses[alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_WRITE])
            flags |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        if(accesses[alloy::ResourceAccess::SHADING_RATE_SOURCE])
            flags |= VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
        if(accesses[alloy::ResourceAccess::NO_ACCESS])
            flags |= VK_ACCESS_NONE;
        
        //if(stages[alloy::PipelineStage::EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO]){  }
        //if(stages[alloy::PipelineStage::BUILD_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        //if(stages[alloy::PipelineStage::COPY_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        return flags;
    }
#endif
    VkImageLayout _GetTexLayout (const alloy::TextureLayout& layout) {
            switch(layout) {
                case alloy::TextureLayout::COMMON: return VK_IMAGE_LAYOUT_GENERAL;
                case alloy::TextureLayout::PRESENT: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                case alloy::TextureLayout::COMMON_READ: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                case alloy::TextureLayout::RENDER_TARGET: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                case alloy::TextureLayout::UNORDERED_ACCESS: return VK_IMAGE_LAYOUT_GENERAL;
                case alloy::TextureLayout::DEPTH_STENCIL_WRITE: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                case alloy::TextureLayout::DEPTH_STENCIL_READ: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                case alloy::TextureLayout::SHADER_RESOURCE: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                case alloy::TextureLayout::COPY_SOURCE: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                case alloy::TextureLayout::COPY_DEST: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                case alloy::TextureLayout::RESOLVE_SOURCE: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                case alloy::TextureLayout::RESOLVE_DEST: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                case alloy::TextureLayout::SHADING_RATE_SOURCE: return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;

                default: return VK_IMAGE_LAYOUT_UNDEFINED;
            }
        };

    
    template<typename T>
    static void _PopulateBarrierAccess(const alloy::MemoryBarrierDescription& desc,
                                        VulkanCommandList* cmdList,
                                              T& barrier
    ) {
        barrier.dstAccessMask = vk_access_flags_from_d3d12_barrier(cmdList, desc.accessAfter);
        barrier.srcAccessMask = vk_access_flags_from_d3d12_barrier(cmdList, desc.accessBefore);
    }
    
    void BindBarrier(VulkanCommandList* cmdBuf, const std::vector<alloy::BarrierDescription>& barriers) {

        struct BarriersInSyncStage {
            VkPipelineStageFlags stagesBefore;
            VkPipelineStageFlags stagesAfter ;
            std::vector<alloy::BarrierDescription> barriers;
        };

        std::vector <BarriersInSyncStage> stageCollections;

        for(auto& b : barriers) {

            auto stagesBefore = vk_stage_flags_from_d3d12_barrier(cmdBuf, b.memBarrier.stagesBefore, b.memBarrier.accessBefore);
            auto stagesAfter = vk_stage_flags_from_d3d12_barrier(cmdBuf, b.memBarrier.stagesAfter, b.memBarrier.accessAfter);

            BarriersInSyncStage *pCollection = nullptr;
            for(auto& s : stageCollections) {
                if(s.stagesBefore == stagesBefore &&
                   s.stagesAfter == stagesAfter) {
                    pCollection = &s;
                    break;
                }
            }

            if(pCollection == nullptr) {
                pCollection = &stageCollections.emplace_back(stagesBefore, stagesAfter);
            }

            pCollection->barriers.push_back(b);
        }

        for(auto& c : stageCollections) {
            std::vector<VkMemoryBarrier> memBarriers;
            std::vector<VkBufferMemoryBarrier> bufBarriers;
            std::vector<VkImageMemoryBarrier> texBarrier;

            for(auto& desc : c.barriers) {
                if(std::holds_alternative<alloy::MemoryBarrierResource>(desc.resourceInfo)) {
                    memBarriers.emplace_back(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
                    auto& barrier = memBarriers.back();
                    _PopulateBarrierAccess(desc.memBarrier, cmdBuf, barrier);
                }
                else if(std::holds_alternative<alloy::BufferBarrierResource>(desc.resourceInfo)) {
                    bufBarriers.emplace_back(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
                    auto& barrier = bufBarriers.back();
                    auto& barrierDesc = std::get<alloy::BufferBarrierResource>(desc.resourceInfo);

                    auto thisBuf = PtrCast<VulkanBuffer>(barrierDesc.resource.get());
                    _PopulateBarrierAccess(desc.memBarrier, cmdBuf, barrier);
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.buffer = thisBuf->GetHandle();
                    //TODO : Currently includes the whole buffer. Maybe add finer grain control later.
                    barrier.offset = 0;
                    barrier.size = thisBuf->GetDesc().sizeInBytes;
                }
                else /*if(std::holds_alternative<alloy::TextureBarrierDescription>(desc))*/ {
                    texBarrier.emplace_back(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
                    auto& barrier = texBarrier.back();
                    auto& texDesc = std::get<alloy::TextureBarrierResource>(desc.resourceInfo);

                    auto thisTex = PtrCast<VulkanTexture>(texDesc.resource.get());
                    auto& vkTexResDesc = thisTex->GetDesc();
                    _PopulateBarrierAccess(desc.memBarrier, cmdBuf, barrier);
                    barrier.oldLayout = _GetTexLayout(texDesc.fromLayout);
                    barrier.newLayout = _GetTexLayout(texDesc.toLayout);
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = thisTex->GetHandle();
                    auto& aspectMask = barrier.subresourceRange.aspectMask;
                    if (vkTexResDesc.usage.depthStencil) {
                        aspectMask = Helpers::FormatHelpers::IsStencilFormat(vkTexResDesc.format)
                            ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                            : VK_IMAGE_ASPECT_DEPTH_BIT;
                    }
                    else {
                        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    }
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = vkTexResDesc.mipLevels;
                    barrier.subresourceRange.baseArrayLayer = 0;
                    barrier.subresourceRange.layerCount = vkTexResDesc.arrayLayers;
                }
            }

            vkCmdPipelineBarrier(
                cmdBuf->GetHandle(),
                c.stagesBefore,
                c.stagesAfter,
                0 /*VkDependencyFlags*/,
                memBarriers.size(), memBarriers.data(),
                bufBarriers.size(), bufBarriers.data(),
                texBarrier.size(), texBarrier.data());

        }
    }


} // namespace Veldrid

