#pragma once

#include "Veldrid/ResourceBarrier.hpp"

#include <volk.h>

namespace Veldrid
{
    
    
    void BindBarrier(VkCommandBuffer cmdBuf, const alloy::BarrierDescription& barriers);

} // namespace Veldrid

