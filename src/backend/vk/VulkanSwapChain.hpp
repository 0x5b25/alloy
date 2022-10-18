#pragma once

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/SwapChain.hpp"

namespace Veldrid
{
    class VulkanDevice;
    
    class VulkanSwapChain : public SwapChain{

        VulkanSwapChain(const sp<VulkanDevice>& dev) : SwapChain(dev){}

    public:

        static sp<SwapChain> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
        );

    };

} // namespace Veldrid

