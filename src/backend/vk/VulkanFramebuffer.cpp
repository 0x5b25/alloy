#include "VulkanFramebuffer.hpp"

#include "veldrid/common/Common.hpp"
#include "veldrid/Helpers.hpp"

#include <vector>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanTexture.hpp"
#include "VulkanDevice.hpp"

namespace Veldrid
{



    void VulkanFramebufferBase::CreateCompatibleRenderPasses(
        VulkanDevice* vkDev,
        const Description& desc,
        bool isPresented,
        VkRenderPass& noClearInit,
        VkRenderPass& noClearLoad,
        VkRenderPass& clear
    ){

        VkRenderPassCreateInfo renderPassCI{};
        renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        std::vector<VkAttachmentDescription> attachments{};

        unsigned colorAttachmentCount = desc.colorTargets.size();
        std::vector<VkAttachmentReference> colorAttachmentRefs{};
        for (int i = 0; i < colorAttachmentCount; i++)
        {
            auto vkColorTex = PtrCast<VulkanTexture>(desc.colorTargets[i].target.get());
            auto& texDesc = vkColorTex->GetDesc();
            VkAttachmentDescription colorAttachmentDesc{};
            colorAttachmentDesc.format = Veldrid::VK::priv::VdToVkPixelFormat(texDesc.format);
            colorAttachmentDesc.samples = Veldrid::VK::priv::VdToVkSampleCount(texDesc.sampleCount);
            colorAttachmentDesc.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
            colorAttachmentDesc.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachmentDesc.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentDesc.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachmentDesc.initialLayout = (texDesc.usage.sampled)
                    ? VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    : VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentDesc.finalLayout = isPresented
                ? VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                : VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
            depthAttachmentDesc.format = Veldrid::VK::priv::VdToVkPixelFormat(texDesc.format);
            depthAttachmentDesc.samples = Veldrid::VK::priv::VdToVkSampleCount(texDesc.sampleCount);
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

        VK_CHECK(vkCreateRenderPass(vkDev->LogicalDev(), &renderPassCI, nullptr, &noClearInit));

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

        VK_CHECK(vkCreateRenderPass(vkDev->LogicalDev(), &renderPassCI, nullptr, &noClearLoad));


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
       
        VK_CHECK(vkCreateRenderPass(vkDev->LogicalDev(), &renderPassCI, nullptr, &clear));

    }

    VulkanFramebufferBase::~VulkanFramebufferBase(){
        auto vkDev = PtrCast<VulkanDevice>(dev.get());

        
    }

    VulkanFramebuffer::~VulkanFramebuffer(){
        auto vkDev = PtrCast<VulkanDevice>(dev.get());
        vkDestroyFramebuffer(vkDev->LogicalDev(), _fb, nullptr);

        vkDestroyRenderPass(vkDev->LogicalDev(), renderPassNoClear, nullptr);
        vkDestroyRenderPass(vkDev->LogicalDev(), renderPassNoClearLoad, nullptr);
        vkDestroyRenderPass(vkDev->LogicalDev(), renderPassClear, nullptr);

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
        

        unsigned colorAttachmentCount = desc.colorTargets.size();

        //Actually create the framebuffer
        VkFramebufferCreateInfo fbCI {};
        fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        unsigned fbAttachmentsCount = colorAttachmentCount;
        if(desc.HasDepthTarget()) fbAttachmentsCount += 1;

        std::vector<VkImageView> fbAttachments{fbAttachmentsCount};
        for (int i = 0; i < colorAttachmentCount; i++)
        {
            VulkanTexture* vkColorTarget = PtrCast<VulkanTexture>(desc.colorTargets[i].target.get());
            VkImageViewCreateInfo imageViewCI{};
            imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCI.image = vkColorTarget->GetHandle();
            imageViewCI.format = Veldrid::VK::priv::VdToVkPixelFormat(vkColorTarget->GetDesc().format);
            imageViewCI.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCI.subresourceRange.baseMipLevel = desc.colorTargets[i].mipLevel;
            imageViewCI.subresourceRange.levelCount = 1;
            imageViewCI.subresourceRange.baseArrayLayer = desc.colorTargets[i].arrayLayer;
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
            depthViewCI.format = Veldrid::VK::priv::VdToVkPixelFormat(vkDepthTarget->GetDesc().format);
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
            dimTex = desc.colorTargets[0].target;
            mipLevel = desc.colorTargets[0].mipLevel;
        }

        auto framebuffer = new VulkanFramebuffer(dev, desc);


        std::uint32_t mipWidth, mipHeight, mipDepth;
        Helpers::GetMipDimensions(dimTex->GetDesc(), mipLevel, mipWidth, mipHeight, mipDepth);

        fbCI.width = mipWidth;
        fbCI.height = mipHeight;

        fbCI.attachmentCount = fbAttachmentsCount;
        fbCI.pAttachments = fbAttachments.data();
        fbCI.layers = 1;
        fbCI.renderPass = framebuffer->renderPassNoClear;

        VkFramebuffer fb;
        VK_CHECK(vkCreateFramebuffer(dev->LogicalDev(), &fbCI, nullptr, &fb));

        framebuffer->_fb = fb;
        framebuffer->_attachmentViews = std::move(fbAttachments);

        return sp<Framebuffer>(framebuffer);
    }


    void VulkanFramebuffer::TransitionToAttachmentLayout(VkCommandBuffer cb) {
        //TODO: Need to support compute shader pipeline stage?
        for (auto& colorDesc : description.colorTargets)
        {
            VulkanTexture* vkColorTarget = PtrCast<VulkanTexture>(colorDesc.target.get());
            vkColorTarget->TransitionImageLayout(cb,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
        }

        // Depth
        if (description.HasDepthTarget())
        {
            VulkanTexture* vkDepthTarget = PtrCast<VulkanTexture>(description.depthTarget.target.get());
            bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(vkDepthTarget->GetDesc().format);

            vkDepthTarget->TransitionImageLayout(cb,
                hasStencil
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
        }
    }

    void VulkanFramebuffer::VisitAttachments(AttachmentVisitor visitor) {
        //TODO: Need to support compute shader pipeline stage?
        for (auto& colorDesc : description.colorTargets)
        {
            sp<VulkanTexture> vkColorTarget = SPCast<VulkanTexture>(colorDesc.target);
            visitor(vkColorTarget, VisitedAttachmentType::ColorAttachment);
        }

        // Depth
        if (description.HasDepthTarget())
        {
            sp<VulkanTexture> vkDepthTarget = SPCast<VulkanTexture>(description.depthTarget.target);
            bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(vkDepthTarget->GetDesc().format);
            auto type = hasStencil
                ? VisitedAttachmentType::DepthStencilAttachment
                : VisitedAttachmentType::DepthAttachment;

            visitor(vkDepthTarget, type);
        }
    }

} // namespace Veldrid
