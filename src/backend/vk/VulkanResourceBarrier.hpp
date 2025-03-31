#pragma once

#include "alloy/ResourceBarrier.hpp"
#include "utils/ResourceStateTracker.hpp"

#include <volk.h>

namespace alloy::vk
{
    class VulkanCommandList;
    class VulkanDevice;

    VkPipelineStageFlags vk_stage_flags_from_d3d12_barrier(
        PipelineStages sync, 
        const ResourceAccesses& access);

    VkAccessFlags2 vk_access_flags_from_d3d12_barrier(
        const ResourceAccesses& access);

    VkImageLayout AlToVkTexLayout (const alloy::TextureLayout& layout);

    void InsertBarrier(
        VulkanDevice* dev,
        VkCommandBuffer cmdBuf,
        const alloy::utils::BarrierActions& barriers);
    
    void BindBarrier(VulkanCommandList* cmdBuf, const std::vector<alloy::BarrierDescription>& barriers);

} // namespace alloy

