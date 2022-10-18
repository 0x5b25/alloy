#include "VulkanFramebuffer.hpp"

#include "veldrid/Helpers.hpp"

#include <vector>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanTexture.hpp"
#include "VulkanDevice.hpp"

namespace Veldrid
{
    VulkanFramebuffer::~VulkanFramebuffer(){
        auto vkDev = PtrCast<VulkanDevice>(dev.get());
        vkDestroyFramebuffer(vkDev->LogicalDev(), _fb, nullptr);

        vkDestroyRenderPass(vkDev->LogicalDev(), _renderPassNoClear, nullptr);
        vkDestroyRenderPass(vkDev->LogicalDev(), _renderPassNoClearLoad, nullptr);
        vkDestroyRenderPass(vkDev->LogicalDev(), _renderPassClear, nullptr);

        for (VkImageView view : _attachmentViews)
        {
            vkDestroyImageView(vkDev->LogicalDev(), view, nullptr);
        }

    }

    sp<Framebuffer> VulkanFramebuffer::Make(
        const sp<VulkanDevice>& dev,
        const Description& desc,
        bool isPresented
    ){
        
        VkRenderPassCreateInfo renderPassCI{};

        std::vector<VkAttachmentDescription> attachments{};

        unsigned colorAttachmentCount = desc.colorTarget.size();
        std::vector<VkAttachmentReference> colorAttachmentRefs{};
        for (int i = 0; i < colorAttachmentCount; i++)
        {
            auto vkColorTex = PtrCast<VulkanTexture>(desc.colorTarget[i].target.get());
            auto& texDesc = vkColorTex->GetDesc();
            VkAttachmentDescription colorAttachmentDesc{};
            colorAttachmentDesc.format = VdToVkPixelFormat(texDesc.format);
            colorAttachmentDesc.samples = VdToVkSampleCount(texDesc.sampleCount);
            colorAttachmentDesc.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
            colorAttachmentDesc.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachmentDesc.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentDesc.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachmentDesc.initialLayout = isPresented
                ? VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                : (texDesc.usage.sampled)
                    ? VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    : VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentDesc.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.push_back(colorAttachmentDesc);

            VkAttachmentReference colorAttachmentRef {};
            colorAttachmentRef.attachment = i;
            colorAttachmentRef.layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentRefs.push_back(colorAttachmentRef);
        }

        VkAttachmentReference depthAttachmentRef{};
        if (desc.HasDepthTarget())
        {
            VkAttachmentDescription depthAttachmentDesc{};
            auto vkDepthTex = PtrCast<VulkanTexture>(desc.depthTarget.target.get());
            auto& texDesc = vkDepthTex->GetDesc();
            bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(texDesc.format);
            depthAttachmentDesc.format = VdToVkPixelFormat(texDesc.format);
            depthAttachmentDesc.samples = VdToVkSampleCount(texDesc.sampleCount);
            depthAttachmentDesc.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
            depthAttachmentDesc.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachmentDesc.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachmentDesc.stencilStoreOp = hasStencil
                ? VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE
                : VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachmentDesc.initialLayout = ((texDesc.usage.sampled) != 0)
                ? VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                : VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachmentDesc.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depthAttachmentRef.attachment = colorAttachmentCount;
            depthAttachmentRef.layout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(depthAttachmentDesc);

        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (desc.HasColorTarget())
        {
            subpass.colorAttachmentCount = colorAttachmentCount;
            subpass.pColorAttachments = colorAttachmentRefs.data();
        }

        if (desc.HasDepthTarget())
        {
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
        }

        VkSubpassDependency subpassDependency {};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstAccessMask 
            = VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT 
            | VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        renderPassCI.attachmentCount = attachments.size();
        renderPassCI.pAttachments = attachments.data();
        renderPassCI.subpassCount = 1;
        renderPassCI.pSubpasses = &subpass;
        renderPassCI.dependencyCount = 1;
        renderPassCI.pDependencies = &subpassDependency;

        VkRenderPass _renderPassNoClear;
        VK_CHECK(vkCreateRenderPass(dev->LogicalDev(), &renderPassCI, nullptr, & _renderPassNoClear));

        for (int i = 0; i < colorAttachmentCount; i++)
        {
            attachments[i].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
            attachments[i].initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        if (desc.HasDepthTarget())
        {
            //The last attachment is depth attachment
            attachments[colorAttachmentCount].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
            attachments[colorAttachmentCount].initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(desc.depthTarget.target->GetDesc().format);
            if (hasStencil)
            {
                attachments[colorAttachmentCount].stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
            }

        }

        VkRenderPass _renderPassNoClearLoad;
        VK_CHECK(vkCreateRenderPass(dev->LogicalDev(), &renderPassCI, nullptr, &_renderPassNoClearLoad));


        // Load version

        for (int i = 0; i < colorAttachmentCount; i++)
        {
            attachments[i].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[i].initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
        }

        if (desc.HasDepthTarget())
        {
            attachments[colorAttachmentCount].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[colorAttachmentCount].initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
            bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(desc.depthTarget.target->GetDesc().format);
            if (hasStencil)
            {
                attachments[colorAttachmentCount].stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
            }
        }
       
        VkRenderPass _renderPassClear;
        VK_CHECK(vkCreateRenderPass(dev->LogicalDev(), &renderPassCI, nullptr, &_renderPassClear));


        //Actually create the framebuffer
        VkFramebufferCreateInfo fbCI {};
        fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        unsigned fbAttachmentsCount = attachments.size();

        std::vector<VkImageView> fbAttachments{fbAttachmentsCount};
        for (int i = 0; i < colorAttachmentCount; i++)
        {
            VulkanTexture* vkColorTarget = PtrCast<VulkanTexture>(desc.colorTarget[i].target.get());
            VkImageViewCreateInfo imageViewCI{};
            imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCI.image = vkColorTarget->GetHandle();
            imageViewCI.format = VdToVkPixelFormat(vkColorTarget->GetDesc().format);
            imageViewCI.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCI.subresourceRange.baseMipLevel = desc.colorTarget[i].mipLevel;
            imageViewCI.subresourceRange.levelCount = 1;
            imageViewCI.subresourceRange.baseArrayLayer = desc.colorTarget[i].arrayLayer;
            imageViewCI.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(dev->LogicalDev(), &imageViewCI, nullptr, &fbAttachments[i]));

            //_attachmentViews.Add(*dest);
        }

        // Depth
        if (desc.HasDepthTarget())
        {
            VulkanTexture* vkDepthTarget = PtrCast<VulkanTexture>(desc.depthTarget.target.get());
            bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(vkDepthTarget->GetDesc().format);
            VkImageViewCreateInfo depthViewCI{};
            depthViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            depthViewCI.image = vkDepthTarget->GetHandle();
            depthViewCI.format = VdToVkPixelFormat(vkDepthTarget->GetDesc().format);
            depthViewCI.viewType = desc.depthTarget.target->GetDesc().arrayLayers > 1
                ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY
                : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            depthViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            if(hasStencil){
                depthViewCI.subresourceRange.aspectMask |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            depthViewCI.subresourceRange.baseMipLevel = desc.depthTarget.mipLevel;
            depthViewCI.subresourceRange.levelCount = 1;
            depthViewCI.subresourceRange.baseArrayLayer = desc.depthTarget.arrayLayer;
            depthViewCI.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(dev->LogicalDev(), &depthViewCI, nullptr, &fbAttachments.back()));

            //_attachmentViews.Add(*dest);
        }

        sp<Texture> dimTex;
        std::uint32_t mipLevel;
        if (desc.HasDepthTarget())
        {
            dimTex = desc.depthTarget.target;
            mipLevel = desc.depthTarget.mipLevel;
            
        }
        else
        {
            //At least we should have a target
            assert(desc.HasColorTarget());
            dimTex = desc.colorTarget[0].target;
            mipLevel = desc.colorTarget[0].mipLevel;
        }

        std::uint32_t mipWidth, mipHeight, mipDepth;
        Helpers::GetMipDimensions(dimTex, mipLevel, mipWidth, mipHeight, mipDepth);

        fbCI.width = mipWidth;
        fbCI.height = mipHeight;

        fbCI.attachmentCount = fbAttachmentsCount;
        fbCI.pAttachments = fbAttachments.data();
        fbCI.layers = 1;
        fbCI.renderPass = _renderPassNoClear;

        VkFramebuffer fb;
        VK_CHECK(vkCreateFramebuffer(dev->LogicalDev(), &fbCI, nullptr, &fb));

        auto framebuffer = new VulkanFramebuffer(dev, desc);
        framebuffer->_fb = fb;
        framebuffer->_renderPassNoClear = _renderPassNoClear;
        framebuffer->_renderPassNoClearLoad = _renderPassNoClearLoad;
        framebuffer->_renderPassClear = _renderPassClear;
        framebuffer->_attachmentViews = std::move(fbAttachments);

        return sp<Framebuffer>(framebuffer);
    }
} // namespace Veldrid