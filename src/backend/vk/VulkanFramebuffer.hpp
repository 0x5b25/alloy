#pragma once

#include "alloy/FrameBuffer.hpp"
#include "alloy/RenderPass.hpp"

#include <volk.h>

#include <vector>
#include <functional>

namespace alloy::vk
{
    class VulkanDevice;
    class VulkanTexture;
    class VulkanTextureView;
    class VulkanRenderTarget;

    class VulkanFrameBufferBase : public IFrameBuffer{
    public:
        //enum class VisitedAttachmentType {
        //    ColorAttachment, DepthAttachment, DepthStencilAttachment
        //};
        //using AttachmentVisitor = std::function<void(const sp<VulkanTexture>&, VisitedAttachmentType)>;

    protected:

        common::sp<VulkanDevice> dev;
        

        VulkanFrameBufferBase(
            const common::sp<VulkanDevice>& dev
        ) : dev(dev)
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

        virtual ~VulkanFrameBufferBase();


        //virtual const VkFramebuffer& GetHandle() const = 0;

        //virtual VkRenderPass GetRenderPassNoClear_Init() const = 0;
        //virtual VkRenderPass GetRenderPassNoClear_Load() const = 0;
        //virtual VkRenderPass GetRenderPassClear() const = 0;

        //virtual void TransitionToAttachmentLayout(VkCommandBuffer cb) = 0;
        virtual void InsertCmdBeginDynamicRendering(VkCommandBuffer cb, const RenderPassAction& actions) = 0;

        //virtual void VisitAttachments(AttachmentVisitor visitor) = 0;

    };

    

    class VulkanFrameBuffer : public VulkanFrameBufferBase{

        //VkFramebuffer _fb;
        //VkRenderPass renderPassNoClear;
        //VkRenderPass renderPassNoClearLoad;
        //VkRenderPass renderPassClear;

        std::vector<common::sp<VulkanTextureView>> _colorTgts;
        common::sp<VulkanTextureView> _dsTgt;

        // /Description description;

        VulkanFrameBuffer(
            const common::sp<VulkanDevice>& dev
        ) 
            : VulkanFrameBufferBase(dev)
        { 

            //CreateCompatibleRenderPasses(
            //    reinterpret_cast<VulkanDevice*>(dev.get()), description, isPresented,
            //    renderPassNoClear, renderPassNoClearLoad, renderPassClear
            //);
        }

    public:
        ~VulkanFrameBuffer();

        static common::sp<IFrameBuffer> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc,
            bool isPresented = false
        );

        //virtual const VkFramebuffer& GetHandle() const override {return _fb;}

        //virtual VkRenderPass GetRenderPassNoClear_Init() const {return renderPassNoClear;}
        //virtual VkRenderPass GetRenderPassNoClear_Load() const {return renderPassNoClearLoad;}
        //virtual VkRenderPass GetRenderPassClear() const {return renderPassClear;}

        virtual OutputDescription GetDesc() override;

        
        bool HasDepthTarget() const {return _dsTgt != nullptr;}

        //virtual void TransitionToAttachmentLayout(VkCommandBuffer cb) override;
        virtual void InsertCmdBeginDynamicRendering(VkCommandBuffer cb, const RenderPassAction& actions) override;

        //virtual void VisitAttachments(AttachmentVisitor visitor);

    };

    class VulkanRenderTarget : public IRenderTarget {
        VulkanTextureView& _rt;
        common::sp<VulkanFrameBuffer> _fb;

    public:
        VulkanRenderTarget (
            const common::sp<VulkanFrameBuffer> fb,
            VulkanTextureView& rt
        )
            : _rt(rt)
            , _fb(fb)
        { }

        
        virtual ITextureView& GetTexture() const override;

    };
} // namespace alloy

