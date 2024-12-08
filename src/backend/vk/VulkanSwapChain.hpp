#pragma once

#include <volk.h>

#include "veldrid/common/Common.hpp"
#include "veldrid/common/RefCnt.hpp"
#include "veldrid/Helpers.hpp"
#include "veldrid/SwapChain.hpp"
#include "veldrid/Framebuffer.hpp"

#include <vector>
#include <optional>

#include "VkCommon.hpp"
#include "VulkanFramebuffer.hpp"
#include "VulkanTexture.hpp"

namespace Veldrid
{
    class VulkanDevice;
    class VulkanSwapChain;
    
    class _SCFB : public VulkanFramebufferBase{

        sp<VulkanSwapChain> _sc;

        sp<VulkanTexture> _colorTarget;
        sp<VulkanTexture> _depthTarget;

        VkImageView _colorTargetView;
        VkImageView _depthTargetView;
        VkFramebuffer _fb;


        _SCFB(const sp<GraphicsDevice>& dev) : VulkanFramebufferBase(dev){}

    public:

        ~_SCFB();

        static sp<_SCFB> Make(
            VulkanDevice* dev,
            const sp<VulkanSwapChain>& sc,
            const sp<VulkanTexture>& colorTarget,
            const sp<VulkanTexture>& depthTarget = nullptr
        );

        virtual const VkFramebuffer& GetHandle() const override {return _fb;}

        virtual VkRenderPass GetRenderPassNoClear_Init() const;
        virtual VkRenderPass GetRenderPassNoClear_Load() const;
        virtual VkRenderPass GetRenderPassClear() const;

        virtual const Description& GetDesc() const;

        bool HasDepthTarget() const {return _depthTarget != nullptr;}

        void SetToInitialLayout() {
            _colorTarget->SetLayout(VK_IMAGE_LAYOUT_UNDEFINED);
        }

        void TransitionToAttachmentLayout(VkCommandBuffer cb) override {
            {
                _colorTarget->TransitionImageLayout(cb,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
            }

            // Depth
            if (_depthTarget)
            {
               bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(_depthTarget->GetDesc().format);

                _depthTarget->TransitionImageLayout(cb,
                    hasStencil
                        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                        : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
            }
        }

        void VisitAttachments(AttachmentVisitor visitor) override {
            visitor(_colorTarget, VisitedAttachmentType::ColorAttachment);
            
            // Depth
            if (_depthTarget)
            {
                bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(_depthTarget->GetDesc().format);
                auto type = hasStencil
                    ? VisitedAttachmentType::DepthStencilAttachment
                    : VisitedAttachmentType::DepthAttachment;
                visitor(_depthTarget, type);
            }
        }

    };

    class VulkanSwapChain : public SwapChain{
        friend class _SCFB;
           
        VkRenderPass _renderPassNoClear;
        VkRenderPass _renderPassNoClearLoad;
        VkRenderPass _renderPassClear;

        sp<VulkanTexture> _depthTarget;
        std::vector<sp<_SCFB>> _fbs;
        Framebuffer::Description _fbDesc;

        VkSwapchainKHR _deviceSwapchain;
        VkExtent2D _scExtent;
        VkSurfaceFormatKHR _surfaceFormat;
        bool _syncToVBlank, _newSyncToVBlank;

        VkFence _imageAvailableFence;
        std::uint32_t _currentImageIndex;

        VulkanSwapChain(
            const sp<GraphicsDevice>& dev,
            const Description& desc
            ) : SwapChain(dev, desc){
            _syncToVBlank = _newSyncToVBlank = desc.syncToVerticalBlank;
        }

        bool CreateSwapchain(std::uint32_t width, std::uint32_t height);

        void ReleaseFramebuffers();
        void CreateFramebuffers();

        void SetImageIndex(std::uint32_t index) {_currentImageIndex = index; }

        VkResult AcquireNextImage(VkSemaphore semaphore, VkFence fence);

        //void RecreateAndReacquire(std::uint32_t width, std::uint32_t height);
        

    public:

        ~VulkanSwapChain();

        static sp<SwapChain> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
        );

        const VkRenderPass& GetRenderPassNoClear_Init() const {return _renderPassNoClear;}
        const VkRenderPass& GetRenderPassNoClear_Load() const {return _renderPassNoClearLoad;}
        const VkRenderPass& GetRenderPassClear() const {return _renderPassClear;}
        const VkSwapchainKHR& GetHandle() const { return _deviceSwapchain; }

        std::uint32_t GetCurrentImageIdx() const { return _currentImageIndex; }

    public:
        sp<Framebuffer> GetBackBuffer() override;

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

} // namespace Veldrid

