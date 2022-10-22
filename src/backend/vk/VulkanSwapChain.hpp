#pragma once

#include <volk.h>

#include "veldrid/common/RefCnt.hpp"
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

        _SCFB(const sp<VulkanDevice>& dev) : VulkanFramebufferBase(dev){}

    public:

        ~_SCFB();

        static sp<_SCFB> Make(
            VulkanDevice* dev,
            const sp<VulkanSwapChain>& sc,
            const sp<VulkanTexture>& colorTarget,
            const sp<VulkanTexture>& depthTarget = nullptr
        );

        
        virtual VkRenderPass GetRenderPassNoClear_Init() const;
        virtual VkRenderPass GetRenderPassNoClear_Load() const;
        virtual VkRenderPass GetRenderPassClear() const;

        virtual const Description& GetDesc() const {return description;}

        bool HasDepthTarget() const {return _depthTarget != nullptr;}

    };

    class VulkanSwapChain : public SwapChain{
   
        VkRenderPass _renderPassNoClear;
        VkRenderPass _renderPassNoClearLoad;
        VkRenderPass _renderPassClear;

        sp<VulkanTexture> _depthTarget;
        std::vector<sp<_SCFB>> _fbs;

        VkSwapchainKHR _deviceSwapchain;
        VkExtent2D _scExtent;
        VkSurfaceFormatKHR _surfaceFormat;
        bool _syncToVBlank, _newSyncToVBlank;

        VkFence _imageAvailableFence;
        std::uint32_t _currentImageIndex;

        VulkanSwapChain(
            const sp<VulkanDevice>& dev,
            const Description& desc
            ) : SwapChain(dev, desc){
            _syncToVBlank = _newSyncToVBlank = desc.syncToVerticalBlank;
        }

        bool CreateSwapchain(std::uint32_t width, std::uint32_t height);

        void ReleaseFramebuffers();
        void CreateFramebuffers();

        void SetImageIndex(std::uint32_t index) {_currentImageIndex = index; }

        bool AcquireNextImage(VkSemaphore semaphore, VkFence fence)
        {
            auto _gd = PtrCast<VulkanDevice>(dev.get());

            if (_newSyncToVBlank != _syncToVBlank)
            {
                _syncToVBlank = _newSyncToVBlank;
                RecreateAndReacquire(GetWidth(), GetHeight());
                return false;
            }

            VkResult result = vkAcquireNextImageKHR(
                _gd->LogicalDev(),
                _deviceSwapchain,
                UINT64_MAX,
                semaphore,
                fence,
                &_currentImageIndex);
            //_framebuffer.SetImageIndex(_currentImageIndex);
            if (    result == VkResult::VK_ERROR_OUT_OF_DATE_KHR 
                 || result == VkResult::VK_SUBOPTIMAL_KHR        )
            {
                CreateSwapchain(GetWidth(), GetHeight());
                return false;
            }
            else if (result != VkResult::VK_SUCCESS)
            {
                //throw new VeldridException("Could not acquire next image from the Vulkan swapchain.");
                assert(false);
                return false;
            }

            return true;
        }

        void RecreateAndReacquire(std::uint32_t width, std::uint32_t height)
        {
            auto _gd = PtrCast<VulkanDevice>(dev.get());
            if (CreateSwapchain(width, height))
            {
                if (AcquireNextImage(VK_NULL_HANDLE, _imageAvailableFence))
                {
                    vkWaitForFences(_gd->LogicalDev(), 1, &_imageAvailableFence, true, UINT64_MAX);
                    vkResetFences(_gd->LogicalDev(), 1, &_imageAvailableFence);
                }
            }
        }
        

    public:

        ~VulkanSwapChain();

        static sp<SwapChain> Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
        );

        const VkRenderPass& GetRenderPassNoClear_Init() const {return _renderPassNoClear;}
        const VkRenderPass& GetRenderPassNoClear_Load() const {return _renderPassNoClearLoad;}
        const VkRenderPass& GetRenderPassClear() const {return _renderPassClear;}

    public:
        sp<Framebuffer> GetFramebuffer() override{
            assert(_currentImageIndex < _fbs.size());
            return _fbs[_currentImageIndex];
        }

        void Resize(
            std::uint32_t width, 
            std::uint32_t height) override
        {
            RecreateAndReacquire(width, height);
        }

        bool IsSyncToVerticalBlank() const override {return _syncToVBlank;}
        void SetSyncToVerticalBlank(bool sync) override {_newSyncToVBlank = sync;}

        bool HasDepthTarget() const {return description.depthFormat.has_value();}

        std::uint32_t GetWidth() const override {return _scExtent.width;}
        std::uint32_t GetHeight() const override {return _scExtent.height;}

    };

} // namespace Veldrid

