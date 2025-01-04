#pragma once

#include "Veldrid/ResourceBarrier.hpp"

#include <volk.h>

namespace Veldrid
{
    class VulkanCommandList;
    
    void BindBarrier(VulkanCommandList* cmdBuf, const std::vector<alloy::BarrierDescription>& barriers);

} // namespace Veldrid

