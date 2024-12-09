#include "VulkanResourceBarrier.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"
#include "Veldrid/common/Common.hpp"

namespace Veldrid
{

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
                                              T& barrier
    ) {
        barrier.dstAccessMask = _GetAccessFlags(desc.accessAfter);
        barrier.srcAccessMask = _GetAccessFlags(desc.accessBefore);
    }
    
    void BindBarrier(VkCommandBuffer cmdBuf, const alloy::BarrierDescription& barriers) {

        VkPipelineStageFlags stagesBefore = _AlToVkStageFlags(barriers.stagesBefore);
        VkPipelineStageFlags stagesAfter = _AlToVkStageFlags(barriers.stagesAfter);

        std::vector<VkMemoryBarrier> memBarriers;
        std::vector<VkBufferMemoryBarrier> bufBarriers;
        std::vector<VkImageMemoryBarrier> texBarrier;

        for(auto& desc : barriers.barriers) {
            if(std::holds_alternative<alloy::MemoryBarrierDescription>(desc)) {
                memBarriers.emplace_back(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
                auto& barrier = memBarriers.back();
                _PopulateBarrierAccess(std::get<alloy::MemoryBarrierDescription>(desc), barrier);
            }
            else if(std::holds_alternative<alloy::BufferBarrierDescription>(desc)) {
                bufBarriers.emplace_back(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
                auto& barrier = bufBarriers.back();
                auto& barrierDesc = std::get<alloy::BufferBarrierDescription>(desc);

                auto thisBuf = PtrCast<VulkanBuffer>(barrierDesc.resource.get());
                _PopulateBarrierAccess(barrierDesc, barrier);
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
                auto& texDesc = std::get<alloy::TextureBarrierDescription>(desc);

                auto thisTex = PtrCast<VulkanTexture>(texDesc.resource.get());
                auto& desc = thisTex->GetDesc();
                _PopulateBarrierAccess(texDesc, barrier);
                barrier.oldLayout = _GetTexLayout(texDesc.fromLayout);
                barrier.newLayout = _GetTexLayout(texDesc.toLayout);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = thisTex->GetHandle();
                auto& aspectMask = barrier.subresourceRange.aspectMask;
                if (desc.usage.depthStencil) {
                    aspectMask = Helpers::FormatHelpers::IsStencilFormat(desc.format)
                        ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                        : VK_IMAGE_ASPECT_DEPTH_BIT;
                }
                else {
                    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                }
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = desc.mipLevels;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = desc.arrayLayers;
            }
        }

        vkCmdPipelineBarrier(
            cmdBuf,
            stagesBefore,
            stagesAfter,
            0 /*VkDependencyFlags*/,
            memBarriers.size(), memBarriers.data(),
            bufBarriers.size(), bufBarriers.data(),
            texBarrier.size(), texBarrier.data());
    }


} // namespace Veldrid

