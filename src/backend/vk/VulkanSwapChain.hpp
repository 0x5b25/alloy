#pragma once

#include <volk.h>

#include "alloy/common/Common.hpp"
#include "alloy/common/RefCnt.hpp"
#include "alloy/Helpers.hpp"
#include "alloy/SwapChain.hpp"

#include <vector>
#include <optional>

#include "VkCommon.hpp"
#include "VkSurfaceUtil.hpp"
#include "VulkanTexture.hpp"

namespace alloy::vk
{
    class VulkanDevice;
    class VulkanSwapChain;

    struct BackBufferContainer {
        //DXCSwapChain* _sc;
        //common::sp<VulkanTextureView> colorTgt;
        
        common::sp<VulkanTexture> colorTgt;
        VkImageView colorTgtView;
        
        //common::sp<VulkanTextureView> dsTgt;

        //IFrameBuffer::Description fbDesc;
    };

    // A non-owning texture view that holds a strong 
    // reference to swapchain. Meant to be constructed
    // on use, not to be cached into swapchain to avoid
    // cyclic references between view and swapchain.
    class VulkanSCTexView : public VulkanTextureViewBase {
        common::sp<VulkanSwapChain> _sc;

    public:

        VulkanSCTexView(
            common::sp<VulkanSwapChain> sc,
            common::sp<VulkanTexture> target,
            VkImageView view,
            ITextureView::Description desc
        )
            : VulkanTextureViewBase(std::move(target), view, std::move(desc))
            , _sc(std::move(sc))
        { }

    };


    class VulkanSwapChain : public ISwapChain{
        friend class _SCFB;

        common::sp<VulkanDevice> _dev;

        VK::priv::SurfaceContainer _surf;
           
        //VkRenderPass _renderPassNoClear;
        //VkRenderPass _renderPassNoClearLoad;
        //VkRenderPass _renderPassClear;

        //common::sp<VulkanTexture> _depthTarget;
        std::vector<BackBufferContainer> _fbs;
        //std::vector<Framebuffer::Description> _fbDesc;

        VkSwapchainKHR _deviceSwapchain;
        VkExtent2D _scExtent;
        VkSurfaceFormatKHR _surfaceFormat;
        bool _syncToVBlank, _newSyncToVBlank;

        VkFence _imageAvailableFence;
        std::uint32_t _currentImageIndex;
        bool _currentImageInUse;

        Description _desc;

        VulkanSwapChain(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        ) 
            : _dev(dev)
            , _desc(desc)
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

        virtual const Description& GetDesc() const override { return _desc; }

        //const VkRenderPass& GetRenderPassNoClear_Init() const {return _renderPassNoClear;}
        //const VkRenderPass& GetRenderPassNoClear_Load() const {return _renderPassNoClearLoad;}
        //const VkRenderPass& GetRenderPassClear() const {return _renderPassClear;}
        const VkSwapchainKHR& GetHandle() const { return _deviceSwapchain; }

        std::uint32_t GetCurrentImageIdx() const { return _currentImageIndex; }

        void MarkCurrentImageInUse() { _currentImageInUse = true; }

    public:
        common::sp<ITextureView> GetBackBuffer() override;

        virtual uint32_t GetBackBufferIndex() override { return GetCurrentImageIdx(); }

        void Resize(
            std::uint32_t width, 
            std::uint32_t height) override
        {
            CreateSwapchain(width, height);
            //RecreateAndReacquire(width, height);
        }

        bool IsSyncToVerticalBlank() const override {return _syncToVBlank;}
        void SetSyncToVerticalBlank(bool sync) override {_newSyncToVBlank = sync;}

        std::uint32_t GetWidth() const override {return _scExtent.width;}
        std::uint32_t GetHeight() const override {return _scExtent.height;}

        //virtual State SwapBuffers() override;
    };

} // namespace alloy

