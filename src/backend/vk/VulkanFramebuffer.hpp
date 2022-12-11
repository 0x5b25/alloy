#pragma once

#include "veldrid/Framebuffer.hpp"

#include <volk.h>

#include <vector>
#include <functional>

namespace Veldrid
{
    class VulkanDevice;
    class VulkanTexture;

    class VulkanFramebufferBase : public Framebuffer{
    public:
        enum class VisitedAttachmentType {
            ColorAttachment, DepthAttachment, DepthStencilAttachment
        };
        using AttachmentVisitor = std::function<void(const sp<VulkanTexture>&, VisitedAttachmentType)>;

    protected:
        

        VulkanFramebufferBase(
            const sp<GraphicsDevice>& dev
        ) : Framebuffer(dev)
        { 
            //CreateCompatibleRenderPasses(
            //    renderPassNoClear, renderPassNoClearLoad, renderPassClear,
            //    isPresented
            //);
        }

    public:
        static void CreateCompatibleRenderPasses(
            VulkanDevice* vkDev,
            const Description& desc,
            bool isPresented,
            VkRenderPass& noClearInit,
            VkRenderPass& noClearLoad,
            VkRenderPass& clear
        );

        virtual ~VulkanFramebufferBase();

        virtual const VkFramebuffer& GetHandle() const = 0;


        virtual VkRenderPass GetRenderPassNoClear_Init() const = 0;
        virtual VkRenderPass GetRenderPassNoClear_Load() const = 0;
        virtual VkRenderPass GetRenderPassClear() const = 0;

        virtual void TransitionToAttachmentLayout(VkCommandBuffer cb) = 0;

        virtual void VisitAttachments(AttachmentVisitor visitor) = 0;

    };

    class VulkanFramebuffer : public VulkanFramebufferBase{

        VkFramebuffer _fb;
        VkRenderPass renderPassNoClear;
        VkRenderPass renderPassNoClearLoad;
        VkRenderPass renderPassClear;

        std::vector<VkImageView> _attachmentViews;

        Description description;

        VulkanFramebuffer(
            const sp<GraphicsDevice>& dev,
            const Description& desc,
            bool isPresented = false
        ) 
            : VulkanFramebufferBase(dev)
            , description(desc)
        { 

            CreateCompatibleRenderPasses(
                reinterpret_cast<VulkanDevice*>(dev.get()), description, isPresented,
                renderPassNoClear, renderPassNoClearLoad, renderPassClear
            );
        }

    public:
        ~VulkanFramebuffer();

        static sp<Framebuffer> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc,
            bool isPresented = false
        );

        virtual const VkFramebuffer& GetHandle() const override {return _fb;}

        virtual VkRenderPass GetRenderPassNoClear_Init() const {return renderPassNoClear;}
        virtual VkRenderPass GetRenderPassNoClear_Load() const {return renderPassNoClearLoad;}
        virtual VkRenderPass GetRenderPassClear() const {return renderPassClear;}

        virtual const Description& GetDesc() const {return description;}

        virtual void TransitionToAttachmentLayout(VkCommandBuffer cb) override;

        virtual void VisitAttachments(AttachmentVisitor visitor);

    };
} // namespace Veldrid

