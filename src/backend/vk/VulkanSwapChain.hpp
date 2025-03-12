#pragma once

#include <volk.h>

#include "alloy/common/Common.hpp"
#include "alloy/common/RefCnt.hpp"
#include "alloy/Helpers.hpp"
#include "alloy/SwapChain.hpp"
#include "alloy/Framebuffer.hpp"

#include <vector>
#include <optional>

#include "VkCommon.hpp"
#include "VkSurfaceUtil.hpp"
#include "VulkanFramebuffer.hpp"
#include "VulkanTexture.hpp"

namespace alloy::vk
{
    class VulkanDevice;
    class VulkanSwapChain;

    struct BackBufferContainer {
        //DXCSwapChain* _sc;
        common::sp<VulkanTextureView> colorTgt;
        
        common::sp<VulkanTextureView> dsTgt;

        //IFrameBuffer::Description fbDesc;
    };


    class VulkanSCRT : public IRenderTarget {
        common::sp<VulkanSwapChain> _sc;
        VulkanTextureView& _rt;
    public:

        VulkanSCRT(
            const common::sp<VulkanSwapChain>& sc,
            VulkanTextureView& rt
        )
            : _sc(sc)
            , _rt(rt) 
        { }
        
        VulkanTextureView& GetVkTexView() const {return _rt;}
        virtual ITextureView& GetTexture() const override {return GetVkTexView();}

    };
    
    class _SCFB : public VulkanFrameBufferBase{

        common::sp<VulkanSwapChain> _sc;
        const BackBufferContainer& _bb;

        //common::sp<VulkanTexture> _colorTarget;
        //common::sp<VulkanTexture> _depthTarget;
        //
        //VkImageView _colorTargetView;
        //VkImageView _depthTargetView;
        //VkFramebuffer _fb;

        
        //IFrameBuffer::Description _fbDesc;

    public:

        _SCFB(
            const common::sp<VulkanDevice>& dev,
            common::sp<VulkanSwapChain>&& sc,
            const BackBufferContainer& bb
        ) 
            : VulkanFrameBufferBase(dev)
            , _sc(std::move(sc))
            , _bb(bb)
        { }


        ~_SCFB();

        //static common::sp<_SCFB> Make(
        //    const common::sp<VulkanDevice>& dev,
        //    const common::sp<VulkanSwapChain>& sc,
        //    const common::sp<VulkanTexture>& colorTarget,
        //    const common::sp<VulkanTexture>& depthTarget = nullptr
        //);

        //virtual const VkFramebuffer& GetHandle() const override {return _fb;}

        //virtual VkRenderPass GetRenderPassNoClear_Init() const;
        //virtual VkRenderPass GetRenderPassNoClear_Load() const;
        //virtual VkRenderPass GetRenderPassClear() const;

        virtual OutputDescription GetDesc() override;

        bool HasDepthTarget() const {return _bb.dsTgt != nullptr;}

        //void SetToInitialLayout() {
        //    _colorTarget->SetLayout(VK_IMAGE_LAYOUT_UNDEFINED);
        //}
        //
        //void TransitionToAttachmentLayout(VkCommandBuffer cb) override {
        //    {
        //        _colorTarget->TransitionImageLayout(cb,
        //            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        //            VK_ACCESS_SHADER_WRITE_BIT,
        //            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        //        );
        //    }
        //
        //    // Depth
        //    if (_depthTarget)
        //    {
        //       bool hasStencil = FormatHelpers::IsStencilFormat(_depthTarget->GetDesc().format);
        //
        //        _depthTarget->TransitionImageLayout(cb,
        //            hasStencil
        //                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        //                : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        //            VK_ACCESS_SHADER_WRITE_BIT,
        //            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        //        );
        //    }
        //}

        virtual void InsertCmdBeginDynamicRendering(VkCommandBuffer cb, const RenderPassAction& actions) override {
            
            assert(actions.colorTargetActions.size() == 1);

            auto& ctAct = actions.colorTargetActions.front();

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
            
            VkRenderingAttachmentInfoKHR colorAttachmentRef{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .pNext = nullptr,
                .imageView = _bb.colorTgt->GetHandle(),
                .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = nullptr,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = _Vd2VkLoadOp(ctAct.loadAction),
                .storeOp = _Vd2VkStoreOp(ctAct.storeAction),
                .clearValue = {.color = {
                    .float32 = {
                        ctAct.clearColor.r,
                        ctAct.clearColor.g,
                        ctAct.clearColor.b,
                        ctAct.clearColor.a
                    }
                }},
            };

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
                assert(actions.depthTargetAction.has_value());

                auto& texDesc = _bb.dsTgt->GetTextureObject()->GetDesc();

                depthAttachment.imageView = _bb.dsTgt->GetHandle();
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

            auto& ctDesc = _bb.colorTgt->GetTextureObject()->GetDesc();

            const VkRenderingInfoKHR render_info {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                .renderArea = VkRect2D{ {0, 0}, {ctDesc.width, ctDesc.height} },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentRef,
                .pDepthAttachment = hasDepth ? &depthAttachment : nullptr,
                .pStencilAttachment = hasStencil ? &stencilAttachment : nullptr,   
            };

            vkCmdBeginRenderingKHR(cb, &render_info);
        }

        //void VisitAttachments(AttachmentVisitor visitor) override {
        //    visitor(_colorTarget, VisitedAttachmentType::ColorAttachment);
        //    
        //    // Depth
        //    if (_depthTarget)
        //    {
        //        bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(_depthTarget->GetDesc().format);
        //        auto type = hasStencil
        //            ? VisitedAttachmentType::DepthStencilAttachment
        //            : VisitedAttachmentType::DepthAttachment;
        //        visitor(_depthTarget, type);
        //    }
        //}

    };

    class VulkanSwapChain : public ISwapChain{
        friend class _SCFB;

        common::sp<VulkanDevice> _dev;

        VK::priv::SurfaceContainer _surf;
           
        //VkRenderPass _renderPassNoClear;
        //VkRenderPass _renderPassNoClearLoad;
        //VkRenderPass _renderPassClear;

        common::sp<VulkanTexture> _depthTarget;
        std::vector<BackBufferContainer> _fbs;
        //std::vector<Framebuffer::Description> _fbDesc;

        VkSwapchainKHR _deviceSwapchain;
        VkExtent2D _scExtent;
        VkSurfaceFormatKHR _surfaceFormat;
        bool _syncToVBlank, _newSyncToVBlank;

        VkFence _imageAvailableFence;
        std::uint32_t _currentImageIndex;
        bool _currentImageInUse;

        VulkanSwapChain(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        ) 
            : ISwapChain(desc)
            , _dev(dev)
        {
            _syncToVBlank = _newSyncToVBlank = desc.syncToVerticalBlank;
            _currentImageIndex = 0;
            _currentImageInUse = true;
        }

        bool CreateSwapchain(std::uint32_t width, std::uint32_t height);

        void ReleaseFramebuffers();
        void CreateFramebuffers();

        void SetImageIndex(std::uint32_t index) {_currentImageIndex = index; }

        VkResult AcquireNextImage(VkSemaphore semaphore, VkFence fence);

        //void RecreateAndReacquire(std::uint32_t width, std::uint32_t height);
        
        //void PerformInitialLayoutTransition();

    public:

        ~VulkanSwapChain();

        static common::sp<ISwapChain> Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        );

        //const VkRenderPass& GetRenderPassNoClear_Init() const {return _renderPassNoClear;}
        //const VkRenderPass& GetRenderPassNoClear_Load() const {return _renderPassNoClearLoad;}
        //const VkRenderPass& GetRenderPassClear() const {return _renderPassClear;}
        const VkSwapchainKHR& GetHandle() const { return _deviceSwapchain; }

        std::uint32_t GetCurrentImageIdx() const { return _currentImageIndex; }
        VulkanTextureView* GetCurrentColorTarget() const { return _fbs[_currentImageIndex].colorTgt.get(); }

        void MarkCurrentImageInUse() { _currentImageInUse = true; }

    public:
        common::sp<IFrameBuffer> GetBackBuffer() override;

        void Resize(
            std::uint32_t width, 
            std::uint32_t height) override
        {
            CreateSwapchain(width, height);
            //RecreateAndReacquire(width, height);
        }

        bool IsSyncToVerticalBlank() const override {return _syncToVBlank;}
        void SetSyncToVerticalBlank(bool sync) override {_newSyncToVBlank = sync;}

        bool HasDepthTarget() const {return description.depthFormat.has_value();}

        std::uint32_t GetWidth() const override {return _scExtent.width;}
        std::uint32_t GetHeight() const override {return _scExtent.height;}

        //virtual State SwapBuffers() override;
    };

} // namespace alloy

