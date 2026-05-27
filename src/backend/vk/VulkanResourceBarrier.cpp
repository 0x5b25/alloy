#include "VulkanResourceBarrier.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"
#include "VulkanCommandList.hpp"
#include "alloy/common/Common.hpp"
#include "alloy/common/BitFlags.hpp"

namespace alloy::vk
{

    

VkPipelineStageFlags vk_stage_flags_from_alloy_barrier(
    alloy::PipelineStages sync, 
    const alloy::ResourceAccesses&)
{
    VkPipelineStageFlags stages = 0;
    static_assert(VK_PIPELINE_STAGE_NONE_KHR == 0);

    if(!sync)
        return VK_PIPELINE_STAGE_NONE_KHR;

    if (sync & alloy::PipelineStage::AllCommands)
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    if (sync & alloy::PipelineStage::AllGraphics)
        stages |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    if (sync & alloy::PipelineStage::AllShaders) {
        stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                  VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
                  VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT |
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                  VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (sync & alloy::PipelineStage::DrawIndirect)
        stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    if (sync & alloy::PipelineStage::VertexInput)
        stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;//VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    if (sync & alloy::PipelineStage::VertexShader)
        stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (sync & alloy::PipelineStage::MeshShader)
        stages |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
    if (sync & alloy::PipelineStage::FragmentShader)
        stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (sync & alloy::PipelineStage::DepthStencil)
        stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    if (sync & alloy::PipelineStage::ColorOutput)
        stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (sync & alloy::PipelineStage::ComputeShader)
        stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (sync & alloy::PipelineStage::RayTracing)
        stages |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    if (sync & alloy::PipelineStage::Copy)
        stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;//VK_PIPELINE_STAGE_COPY_BIT;
    if (sync & alloy::PipelineStage::BuildAS)
        stages |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

    return stages;
}

    VkAccessFlags2 vk_access_flags_from_alloy_barrier(
        const alloy::ResourceAccesses& access)
    {
        VkAccessFlags2 vk_access = 0;
    
        if (!access)
            return VK_ACCESS_NONE;
    
        if (access & alloy::ResourceAccess::IndirectArgumentRead)
            vk_access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        if (access & alloy::ResourceAccess::VertexBufferRead)
            vk_access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        if (access & alloy::ResourceAccess::IndexBufferRead)
            vk_access |= VK_ACCESS_INDEX_READ_BIT;
        if (access & alloy::ResourceAccess::ConstantBufferRead)
            vk_access |= VK_ACCESS_UNIFORM_READ_BIT;
        if (access & alloy::ResourceAccess::ShaderResourceRead)
            vk_access |= VK_ACCESS_SHADER_READ_BIT;
        if (access & alloy::ResourceAccess::UnorderedAccess)
            vk_access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        if (access & alloy::ResourceAccess::RenderTarget)
            vk_access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (access & alloy::ResourceAccess::DepthStencilRead)
            vk_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (access & alloy::ResourceAccess::DepthStencilWrite)
            vk_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (access & alloy::ResourceAccess::CopySource)
            vk_access |= VK_ACCESS_TRANSFER_READ_BIT;
        if (access & alloy::ResourceAccess::CopyDest)
            vk_access |= VK_ACCESS_TRANSFER_WRITE_BIT;
        if (access & alloy::ResourceAccess::AccelerationStructureRead)
            vk_access |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        if (access & alloy::ResourceAccess::AccelerationStructureWrite)
            vk_access |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    
        return vk_access;
    }

    VkImageLayout AlToVkTexLayout (const alloy::TextureLayout& layout) {
        switch(layout) {
            case alloy::TextureLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
            case alloy::TextureLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
            case alloy::TextureLayout::ShaderReadOnly: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            case alloy::TextureLayout::Storage: return VK_IMAGE_LAYOUT_GENERAL;
            case alloy::TextureLayout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case alloy::TextureLayout::DepthStencilReadOnly: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case alloy::TextureLayout::DepthStencilWrite: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case alloy::TextureLayout::CopySource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case alloy::TextureLayout::CopyDest: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case alloy::TextureLayout::ResolveSource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case alloy::TextureLayout::ResolveDest: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case alloy::TextureLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            default: return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    
    template<typename T>
    static void _PopulateBarrierAccess(const alloy::ResourceAccessMask& from,
                                       const alloy::ResourceAccessMask& to,
                                       T& barrier
    ) {
        barrier.dstAccessMask = vk_access_flags_from_alloy_barrier(to);
        barrier.srcAccessMask = vk_access_flags_from_alloy_barrier(from);
    }
    
    void BindBarrier(VulkanCommandList* cmdBuf, std::span<const alloy::BarrierOp> barriers) {
        VkPipelineStageFlags stagesBefore = 0;
        VkPipelineStageFlags stagesAfter = 0;
        std::vector<VkBufferMemoryBarrier> bufBarriers;
        std::vector<VkImageMemoryBarrier> texBarriers;

        for(auto& desc : barriers) {
            if(std::holds_alternative<alloy::BufferBarrierOp>(desc)) {
                auto& barrierDesc = std::get<alloy::BufferBarrierOp>(desc);
                auto thisBuf = common::PtrCast<VulkanBuffer>(barrierDesc.buffer->GetBufferObject().get());
                const auto& shape = barrierDesc.buffer->GetShape();
                auto& barrier = bufBarriers.emplace_back(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);

                stagesBefore |= vk_stage_flags_from_alloy_barrier(
                    barrierDesc.from.stages, barrierDesc.from.access);
                stagesAfter |= vk_stage_flags_from_alloy_barrier(
                    barrierDesc.to.stages, barrierDesc.to.access);

                _PopulateBarrierAccess(barrierDesc.from.access, barrierDesc.to.access, barrier);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = thisBuf->GetHandle();
                barrier.offset = shape.GetOffsetInBytes();
                barrier.size = shape.GetSizeInBytes();
            }
            else {
                auto& barrierDesc = std::get<alloy::TextureBarrierOp>(desc);
                auto texture = barrierDesc.texture->GetTextureObject();
                const auto& texViewDesc = barrierDesc.texture->GetDesc();
                auto thisTex = common::PtrCast<VulkanTexture>(texture.get());
                const auto& texDesc = thisTex->GetDesc();
                auto& barrier = texBarriers.emplace_back(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);

                stagesBefore |= vk_stage_flags_from_alloy_barrier(
                    barrierDesc.from.stages, barrierDesc.from.access);
                stagesAfter |= vk_stage_flags_from_alloy_barrier(
                    barrierDesc.to.stages, barrierDesc.to.access);

                _PopulateBarrierAccess(barrierDesc.from.access, barrierDesc.to.access, barrier);
                barrier.oldLayout = AlToVkTexLayout(barrierDesc.from.layout);
                barrier.newLayout = AlToVkTexLayout(barrierDesc.to.layout);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = thisTex->GetHandle();

                auto& aspectMask = barrier.subresourceRange.aspectMask;
                if (texDesc.usage.depthStencil) {
                    aspectMask = FormatHelpers::IsStencilFormat(texDesc.format)
                        ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                        : VK_IMAGE_ASPECT_DEPTH_BIT;
                }
                else {
                    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                }
                barrier.subresourceRange.baseMipLevel = texViewDesc.baseMipLevel;
                barrier.subresourceRange.levelCount = texViewDesc.mipLevels;
                barrier.subresourceRange.baseArrayLayer = texViewDesc.baseArrayLayer;
                barrier.subresourceRange.layerCount = texViewDesc.arrayLayers;
            }
        }

        if(!bufBarriers.empty() || !texBarriers.empty()) {
            VK_DEV_CALL(cmdBuf->GetDevice(),
                vkCmdPipelineBarrier(
                    cmdBuf->GetHandle(),
                    stagesBefore ? stagesBefore : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    stagesAfter ? stagesAfter : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0,
                    0, nullptr,
                    bufBarriers.size(), bufBarriers.data(),
                    texBarriers.size(), texBarriers.data()));
        }

    }

    
#if 0
    void InsertBarrier(
        VulkanDevice* dev,
        VkCommandBuffer cmdBuf,
        const alloy::utils::BarrierActions& barriers
    ) {
        return;
        if(barriers.bufferBarrierActions.empty() && 
           barriers.textureBarrierActions.empty())
            return;
        struct BarriersInSyncStage {
            VkPipelineStageFlags stagesBefore;
            VkPipelineStageFlags stagesAfter;
            alloy::utils::BarrierActions barriers;
        };

        std::vector <BarriersInSyncStage> stageCollections;

        auto _FindStageForAction = [&](auto action)->BarriersInSyncStage* {
            VkPipelineStageFlags stagesBefore = VK_PIPELINE_STAGE_NONE_KHR;
            if(action.before.stage != PipelineStage::None)
                stagesBefore = vk_stage_flags_from_alloy_barrier(action.before.stage, action.before.access);
            VkPipelineStageFlags stagesAfter = VK_PIPELINE_STAGE_NONE_KHR;
            if(action.after.stage != PipelineStage::None)
                stagesAfter = vk_stage_flags_from_alloy_barrier(action.after.stage, action.after.access);

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

            return pCollection;
        };

        for(auto& b : barriers.bufferBarrierActions) {
            auto stage = _FindStageForAction(b);
            stage->barriers.bufferBarrierActions.push_back(b);
        }

        
        for(auto& b : barriers.textureBarrierActions) {
            auto stage = _FindStageForAction(b);
            stage->barriers.textureBarrierActions.push_back(b);
        }

        auto _GetAccessFlags = [](alloy::ResourceAccess access) -> VkAccessFlags2 {
            if(access == ResourceAccess::None)
                return VK_ACCESS_NONE;
            return vk_access_flags_from_alloy_barrier(access);
        };

        for(auto& c : stageCollections) {
            std::vector<VkMemoryBarrier> memBarriers;
            std::vector<VkBufferMemoryBarrier> bufBarriers;
            std::vector<VkImageMemoryBarrier> texBarrier;

            
            for(auto& desc : c.barriers.bufferBarrierActions) {
                bufBarriers.emplace_back(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
                auto& barrier = bufBarriers.back();

                auto thisBuf = static_cast<VulkanBuffer*>(desc.buffer);
                
                barrier.srcAccessMask = _GetAccessFlags(desc.before.access);
                barrier.dstAccessMask = _GetAccessFlags(desc.after.access);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = thisBuf->GetHandle();
                //TODO : Currently includes the whole buffer. Maybe add finer grain control later.
                barrier.offset = 0;
                barrier.size = thisBuf->GetDesc().sizeInBytes;
            }

            for(auto& desc : c.barriers.textureBarrierActions) {
                texBarrier.emplace_back(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
                auto& barrier = texBarrier.back();

                auto thisTex =static_cast<VulkanTexture*>(desc.texture);
                auto& vkTexResDesc = thisTex->GetDesc();

                barrier.srcAccessMask = _GetAccessFlags(desc.before.access);
                barrier.dstAccessMask = _GetAccessFlags(desc.after.access);
                barrier.oldLayout = AlToVkTexLayout(desc.before.layout);
                barrier.newLayout = AlToVkTexLayout(desc.after.layout);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = thisTex->GetHandle();
                auto& aspectMask = barrier.subresourceRange.aspectMask;
                if (vkTexResDesc.usage.depthStencil) {
                    aspectMask = FormatHelpers::IsStencilFormat(vkTexResDesc.format)
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

            
            VK_DEV_CALL(dev,
                vkCmdPipelineBarrier(
                    cmdBuf,
                    c.stagesBefore,
                    c.stagesAfter,
                    0 /*VkDependencyFlags*/,
                    memBarriers.size(), memBarriers.data(),
                    bufBarriers.size(), bufBarriers.data(),
                    texBarrier.size(), texBarrier.data()));

        }
    }

#endif

} // namespace alloy

