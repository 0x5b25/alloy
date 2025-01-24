#include "VulkanFrameBuffer.hpp"

#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

#include <vector>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanTexture.hpp"
#include "VulkanDevice.hpp"

namespace alloy::vk
{
#if 0
    void VulkanFrameBufferBase::CreateCompatibleRenderPasses(
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
            auto vkColorTex = PtrCast<VulkanTexture>(desc.colorTargets[i]->GetTarget().get());
            auto& texDesc = vkColorTex->GetDesc();
            VkAttachmentDescription colorAttachmentDesc{};
            colorAttachmentDesc.format = VdToVkPixelFormat(texDesc.format);
            colorAttachmentDesc.samples = VdToVkSampleCount(texDesc.sampleCount);
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
            bool hasStencil = FormatHelpers::IsStencilFormat(texDesc.format);
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
            bool hasStencil = FormatHelpers::IsStencilFormat(desc.depthTarget.target->GetDesc().format);
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
            bool hasStencil = FormatHelpers::IsStencilFormat(desc.depthTarget.target->GetDesc().format);
            if (hasStencil)
            {
                attachments[colorAttachmentCount].stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
            }
        }
       
        VK_CHECK(vkCreateRenderPass(vkDev->LogicalDev(), &renderPassCI, nullptr, &clear));

    }
#endif
    VulkanFrameBufferBase::~VulkanFrameBufferBase(){
        auto vkDev = PtrCast<VulkanDevice>(dev.get());

        
    }

    VulkanFrameBuffer::~VulkanFrameBuffer(){
        //auto vkDev = PtrCast<VulkanDevice>(dev.get());
        //vkDestroyFramebuffer(vkDev->LogicalDev(), _fb, nullptr);

        //vkDestroyRenderPass(vkDev->LogicalDev(), renderPassNoClear, nullptr);
        //vkDestroyRenderPass(vkDev->LogicalDev(), renderPassNoClearLoad, nullptr);
        //vkDestroyRenderPass(vkDev->LogicalDev(), renderPassClear, nullptr);

        //for (VkImageView view : _attachmentViews)
        //{
        //    vkDestroyImageView(vkDev->LogicalDev(), view, nullptr);
        //}

    }

    common::sp<IFrameBuffer> VulkanFrameBuffer::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc,
        bool isPresented
    ){
        

        unsigned colorAttachmentCount = desc.colorTargets.size();

        //Actually create the framebuffer
        //VkFramebufferCreateInfo fbCI {};
        //fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        unsigned fbAttachmentsCount = colorAttachmentCount;
        if(desc.depthTarget) fbAttachmentsCount += 1;

        std::vector<common::sp<VulkanTextureView>> colorAttachments;
        for (int i = 0; i < colorAttachmentCount; i++)
        {
            auto vkColorTarget = SPCast<VulkanTexture>(desc.colorTargets[i]->GetTextureObject());
            //VkImageViewCreateInfo imageViewCI{};
            //imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            //imageViewCI.image = vkColorTarget->GetHandle();
            //imageViewCI.format = VdToVkPixelFormat(vkColorTarget->GetDesc().format);
            //imageViewCI.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            //imageViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            //imageViewCI.subresourceRange.baseMipLevel = desc.colorTargets[i].mipLevel;
            //imageViewCI.subresourceRange.levelCount = 1;
            //imageViewCI.subresourceRange.baseArrayLayer = desc.colorTargets[i].arrayLayer;
            //imageViewCI.subresourceRange.layerCount = 1;
            if(!vkColorTarget->GetDesc().usage.renderTarget) {
                assert(false);
            }

            ITextureView::Description ctvDesc {};
            ctvDesc.baseMipLevel = desc.colorTargets[i]->GetDesc().baseMipLevel;
            ctvDesc.mipLevels = 1;
            ctvDesc.baseArrayLayer = desc.colorTargets[i]->GetDesc().baseArrayLayer;
            ctvDesc.arrayLayers = 1;
            auto ctView = VulkanTextureView::Make(vkColorTarget, ctvDesc);

            //VK_CHECK(vkCreateImageView(dev->LogicalDev(), &imageViewCI, nullptr, &fbAttachments[i]));
            colorAttachments.push_back(ctView);
            //_attachmentViews.Add(*dest);
        }

        // Depth
        common::sp<VulkanTextureView> dsAttachment;
        if (desc.depthTarget)
        {
            auto vkDepthTarget = SPCast<VulkanTexture>(desc.depthTarget->GetTextureObject());
            bool hasStencil = FormatHelpers::IsStencilFormat(vkDepthTarget->GetDesc().format);

            if(!vkDepthTarget->GetDesc().usage.depthStencil) {
                assert(false);
            }

            //VkImageViewCreateInfo depthViewCI{};
            //depthViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            //depthViewCI.image = vkDepthTarget->GetHandle();
            //depthViewCI.format = VdToVkPixelFormat(vkDepthTarget->GetDesc().format);
            //depthViewCI.viewType = desc.depthTarget.target->GetDesc().arrayLayers > 1
            //    ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY
            //    : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            //depthViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            //if(hasStencil){
            //    depthViewCI.subresourceRange.aspectMask |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            //}
            //depthViewCI.subresourceRange.baseMipLevel = desc.depthTarget.mipLevel;
            //depthViewCI.subresourceRange.levelCount = 1;
            //depthViewCI.subresourceRange.baseArrayLayer = desc.depthTarget.arrayLayer;
            //depthViewCI.subresourceRange.layerCount = 1;
            //
            //VK_CHECK(vkCreateImageView(dev->LogicalDev(), &depthViewCI, nullptr, &fbAttachments.back()));

            ITextureView::Description dsvDesc {};
            dsvDesc.baseMipLevel = desc.depthTarget->GetDesc().baseMipLevel;
            dsvDesc.mipLevels = 1;
            dsvDesc.baseArrayLayer = desc.depthTarget->GetDesc().baseArrayLayer;
            dsvDesc.arrayLayers = 1;
            dsAttachment = VulkanTextureView::Make(vkDepthTarget, dsvDesc);

            //_attachmentViews.Add(*dest);
        }

        auto framebuffer = new VulkanFrameBuffer(dev);

        //VkFramebuffer fb;
        //VK_CHECK(vkCreateFramebuffer(dev->LogicalDev(), &fbCI, nullptr, &fb));
        //
        //framebuffer->_fb = fb;
        framebuffer->_colorTgts = std::move(colorAttachments);
        framebuffer->_dsTgt = dsAttachment;

        return common::sp<IFrameBuffer>(framebuffer);
    }

#if 0
    void VulkanFrameBuffer::TransitionToAttachmentLayout(VkCommandBuffer cb) {
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
            bool hasStencil = FormatHelpers::IsStencilFormat(vkDepthTarget->GetDesc().format);

            vkDepthTarget->TransitionImageLayout(cb,
                hasStencil
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
        }
    }
#endif
    //void VulkanFrameBuffer::VisitAttachments(AttachmentVisitor visitor) {
    //    //TODO: Need to support compute shader pipeline stage?
    //    for (auto& colorDesc : description.colorTargets)
    //    {
    //        sp<VulkanTexture> vkColorTarget = SPCast<VulkanTexture>(colorDesc.target);
    //        visitor(vkColorTarget, VisitedAttachmentType::ColorAttachment);
    //    }
    //
    //    // Depth
    //    if (description.HasDepthTarget())
    //    {
    //        sp<VulkanTexture> vkDepthTarget = SPCast<VulkanTexture>(description.depthTarget.target);
    //        bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(vkDepthTarget->GetDesc().format);
    //        auto type = hasStencil
    //            ? VisitedAttachmentType::DepthStencilAttachment
    //            : VisitedAttachmentType::DepthAttachment;
    //
    //        visitor(vkDepthTarget, type);
    //    }
    //}

    
    void VulkanFrameBuffer::InsertCmdBeginDynamicRendering(VkCommandBuffer cb, const RenderPassAction& actions) {

        auto _Vd2VkLoadOp = [](alloy::LoadAction load) {
            switch(load) {
                case alloy::LoadAction::Load : return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
                case alloy::LoadAction::Clear : return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
            }
            return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        };

        auto _Vd2VkStoreOp = [](alloy::StoreAction store) {
            switch(store) {
                case alloy::StoreAction::Store : return  VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            }
            return VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
        };

        
        uint32_t width = 0, height = 0;


        unsigned colorAttachmentCount = _colorTgts.size();
        assert(actions.colorTargetActions.size() == colorAttachmentCount);
        std::vector<VkRenderingAttachmentInfoKHR> colorAttachmentRefs{};
        for (int i = 0; i < colorAttachmentCount; i++)
        {
            auto& ctAct = actions.colorTargetActions[i];

            auto vkColorTex = PtrCast<VulkanTexture>(_colorTgts[i]->GetTextureObject().get());
            auto& texDesc = vkColorTex->GetDesc();

            width = std::max(width, texDesc.width);
            height = std::max(height, texDesc.height);

            auto& colorAttachmentDesc 
                = colorAttachmentRefs.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR);

            colorAttachmentDesc.imageView = _colorTgts[i]->GetHandle();
            colorAttachmentDesc.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentDesc.resolveMode = VK_RESOLVE_MODE_NONE;
            colorAttachmentDesc.resolveImageView = nullptr;
            colorAttachmentDesc.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachmentDesc.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
            colorAttachmentDesc.loadOp = _Vd2VkLoadOp(ctAct.loadAction),
            colorAttachmentDesc.storeOp = _Vd2VkStoreOp(ctAct.storeAction),
            colorAttachmentDesc.clearValue = {.color = {
                    .float32 = {
                        ctAct.clearColor.r,
                        ctAct.clearColor.g,
                        ctAct.clearColor.b,
                        ctAct.clearColor.a
                    }
                }};
        }

        VkRenderingAttachmentInfoKHR depthAttachment{
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR
        };

        VkRenderingAttachmentInfoKHR stencilAttachment{
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR
        };

        bool hasDepth = false, hasStencil = false;

        if (HasDepthTarget())
        {
            hasDepth = true;

            auto vkDepthTex = PtrCast<VulkanTexture>(_dsTgt->GetTextureObject().get());
            auto& texDesc = vkDepthTex->GetDesc();
            width = std::max(width, texDesc.width);
            height = std::max(height, texDesc.height);
            
            depthAttachment.imageView = _dsTgt->GetHandle();
            depthAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
            depthAttachment.resolveImageView = nullptr;
            depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.loadOp = _Vd2VkLoadOp(actions.depthTargetAction->loadAction);
            depthAttachment.storeOp = _Vd2VkStoreOp(actions.depthTargetAction->storeAction);
            depthAttachment.clearValue = {
                .depthStencil = {
                    .depth = actions.depthTargetAction->clearDepth
                }
            };

            hasStencil = FormatHelpers::IsStencilFormat(texDesc.format);
            if(hasStencil) {
                stencilAttachment = depthAttachment;
                stencilAttachment.loadOp = _Vd2VkLoadOp(actions.stencilTargetAction->loadAction);
                stencilAttachment.storeOp = _Vd2VkStoreOp(actions.stencilTargetAction->storeAction);
                stencilAttachment.clearValue = {
                    .depthStencil = {
                        .stencil = actions.stencilTargetAction->clearStencil
                    }
                };
            }
        }

        const VkRenderingInfoKHR render_info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .renderArea = VkRect2D{ {0, 0}, {width, height} },
            .layerCount = 1,
            .colorAttachmentCount = (uint32_t)colorAttachmentRefs.size(),
            .pColorAttachments = colorAttachmentRefs.data(),
            .pDepthAttachment = hasDepth ? &depthAttachment : nullptr,
            .pStencilAttachment = hasStencil ? &stencilAttachment : nullptr,   
        };

        vkCmdBeginRenderingKHR(cb, &render_info);
    }

    
    OutputDescription VulkanFrameBuffer::GetDesc() {
        OutputDescription desc { };
        desc.sampleCount = SampleCount::x1;

        for(auto& ct : _colorTgts) {
            desc.colorAttachments.push_back(common::sp(new VulkanRenderTarget(
                common::sp(this),
                *ct.get()
            )));
            ref();

            desc.sampleCount = ct->GetTextureObject()->GetDesc().sampleCount;
        }

        if(HasDepthTarget()) {
            desc.depthAttachment.reset(new VulkanRenderTarget(
                common::sp(this),
                *_dsTgt.get()
            ));
            
            ref();
        }

        return desc;
    }

    
    ITextureView& VulkanRenderTarget::GetTexture() const {return _rt;}

} // namespace alloy
