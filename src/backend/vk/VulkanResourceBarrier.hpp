#pragma once

#include "alloy/ResourceBarrier.hpp"

#include <volk.h>

namespace alloy::vk
{
    class VulkanCommandList;
    
    void BindBarrier(VulkanCommandList* cmdBuf, const std::vector<alloy::BarrierDescription>& barriers);

} // namespace alloy

