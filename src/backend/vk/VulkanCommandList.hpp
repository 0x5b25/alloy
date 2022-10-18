#pragma once

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/CommandList.hpp"

namespace Veldrid
{
    class VulkanDevice;

    class VulkanCommandList : public CommandList{

        VulkanCommandList(const sp<VulkanDevice>& dev) : CommandList(dev){}

    public:
        ~VulkanCommandList();

        static sp<CommandList> Make(const sp<VulkanDevice>& dev);

    };
    

} // namespace Veldrid


