#pragma once

#include "alloy/ResourceBarrier.hpp"

#include <volk.h>
#include <span>

namespace alloy::vk
{
    class VulkanCommandList;
    class VulkanDevice;

    VkPipelineStageFlags vk_stage_flags_from_alloy_barrier(
        PipelineStages sync, 
        const ResourceAccesses& access);

    VkAccessFlags2 vk_access_flags_from_alloy_barrier(
        const ResourceAccesses& access);

    VkImageLayout AlToVkTexLayout (const alloy::TextureLayout& layout);
#if 0
    void InsertBarrier(
        VulkanDevice* dev,
        VkCommandBuffer cmdBuf,
        const alloy::utils::BarrierActions& barriers);
#endif    
    void BindBarrier(VulkanCommandList* cmdBuf, std::span<const alloy::BarrierOp> barriers);

} // namespace alloy

