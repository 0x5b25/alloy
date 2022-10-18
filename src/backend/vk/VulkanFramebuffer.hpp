#pragma once

#include "veldrid/Framebuffer.hpp"

#include <volk.h>

#include <vector>

namespace Veldrid
{
    class VulkanDevice;

    class VulkanFramebuffer : public Framebuffer{

        VkFramebuffer _fb;

        VkRenderPass _renderPassNoClear;
        VkRenderPass _renderPassNoClearLoad;
        VkRenderPass _renderPassClear;

        std::vector<VkImageView> _attachmentViews;

        VulkanFramebuffer(
            const sp<VulkanDevice>& dev,
            const Description& desc
        ) : Framebuffer(dev, desc)
        { }

    public:
        ~VulkanFramebuffer();

        static sp<Framebuffer> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc,
            bool isPresented = false
        );

    };
} // namespace Veldrid

