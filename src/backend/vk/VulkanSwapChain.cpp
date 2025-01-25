#include "VulkanSwapChain.hpp"

#include <volk.h>

#include "alloy/common/Macros.h"

#include <cassert>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VkSurfaceUtil.hpp"
#include "VulkanDevice.hpp"

namespace alloy::vk
{
    bool QueueSupportsPresent(VulkanDevice* dev, std::uint32_t queueFamilyIndex, VkSurfaceKHR surface)
    {
        VkBool32 supported;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
            dev->PhysicalDev(),
            queueFamilyIndex,
            surface,
            &supported));
        
        return supported;
    }

    //bool GetPresentQueueIndex(VulkanDevice* dev, std::uint32_t& queueFamilyIndex)
    //{
    //    auto& phyDevInfo = dev->GetPhyDevInfo();
    //    std::uint32_t graphicsQueueIndex = phyDevInfo.graphicsQueueFamily;
    //    std::uint32_t presentQueueIndex = phyDevInfo.graphicsQueueFamily;
    //    auto& surface = dev->Surface();
    //
    //    if (QueueSupportsPresent(dev, graphicsQueueIndex, surface))
    //    {
    //        queueFamilyIndex = graphicsQueueIndex;
    //        return true;
    //    }
    //    else if (graphicsQueueIndex != presentQueueIndex && QueueSupportsPresent(dev, presentQueueIndex, surface))
    //    {
    //        queueFamilyIndex = presentQueueIndex;
    //        return true;
    //    }
    //
    //    queueFamilyIndex = 0;
    //    return false;
    //}
     
    //VkRenderPass _SCFB::GetRenderPassNoClear_Init() const {return _sc->GetRenderPassNoClear_Init();}
    //VkRenderPass _SCFB::GetRenderPassNoClear_Load() const {return _sc->GetRenderPassNoClear_Load();}
    //VkRenderPass _SCFB::GetRenderPassClear() const {return _sc->GetRenderPassClear();}

    OutputDescription _SCFB::GetDesc() { 
        
        OutputDescription desc { };

        desc.colorAttachments.push_back(common::sp(new VulkanSCRT(
            _sc,
            *_bb.colorTgt.get()
        )));

        if(HasDepthTarget()) {
            desc.depthAttachment.reset(new VulkanSCRT(
                _sc,
                *_bb.dsTgt.get()
            ));
        }

        desc.sampleCount = _bb.colorTgt.get()->GetTextureObject()->GetDesc().sampleCount;

        return desc;
    
    }

    _SCFB::~_SCFB(){
        //auto _gd = PtrCast<VulkanDevice>(dev.get());
        //vkDestroyImageView(_gd->LogicalDev(), _colorTargetView, nullptr);
        //if(HasDepthTarget()){
        //    vkDestroyImageView(_gd->LogicalDev(), _depthTargetView, nullptr);
        //}
        //vkDestroyFramebuffer(_gd->LogicalDev(), _fb, nullptr);
    }


#if 0
    common::sp<_SCFB> _SCFB::_Make(
        const common::sp<VulkanDevice>& dev,
        const common::sp<VulkanSwapChain>& sc,
        const common::sp<VulkanTexture>& colorTarget,
        const common::sp<VulkanTexture>& depthTarget
    ){

        VkImageView attachments[] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        //create image views
        {//color target first
            VkImageViewCreateInfo imageViewCI{};
            imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCI.image = colorTarget->GetHandle();

            auto format =  colorTarget->GetDesc().format;
            imageViewCI.format = alloy::vk::VdToVkPixelFormat(format, false); //not a depth format
            imageViewCI.viewType = colorTarget->GetDesc().arrayLayers > 1
                ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY
                : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCI.subresourceRange.baseMipLevel = 0;
            imageViewCI.subresourceRange.levelCount = colorTarget->GetDesc().mipLevels;
            imageViewCI.subresourceRange.baseArrayLayer = 0;
            imageViewCI.subresourceRange.layerCount = colorTarget->GetDesc().arrayLayers;
            VK_CHECK(vkCreateImageView(dev->LogicalDev(), &imageViewCI, nullptr, &attachments[0]));
        }

        if(depthTarget != nullptr)
        {//depth target
            bool hasStencil = FormatHelpers::IsStencilFormat(depthTarget->GetDesc().format);
            VkImageViewCreateInfo depthViewCI{};
            depthViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            depthViewCI.image = depthTarget->GetHandle();
            depthViewCI.format = alloy::vk::VdToVkPixelFormat(depthTarget->GetDesc().format);
            depthViewCI.viewType = depthTarget->GetDesc().arrayLayers > 1
                ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY
                : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            depthViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            if(hasStencil){
                depthViewCI.subresourceRange.aspectMask |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            depthViewCI.subresourceRange.baseMipLevel = 0;
            depthViewCI.subresourceRange.levelCount = depthTarget->GetDesc().mipLevels;
            depthViewCI.subresourceRange.baseArrayLayer = 0;
            depthViewCI.subresourceRange.layerCount = depthTarget->GetDesc().arrayLayers;
            VK_CHECK(vkCreateImageView(dev->LogicalDev(), &depthViewCI, nullptr, &attachments[1]));
        }

        //VkFramebufferCreateInfo fbCI{};
        //fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        //fbCI.renderPass = sc->GetRenderPassNoClear_Init();
        //fbCI.attachmentCount = 1;
        //fbCI.pAttachments = attachments;
        //fbCI.width = colorTarget->GetDesc().width;
        //fbCI.height = colorTarget->GetDesc().height;
        //fbCI.layers = 1;
        //if(depthTarget != nullptr){
        //    fbCI.attachmentCount += 1;
        //}
        //VkFramebuffer fb;
        //VK_CHECK(vkCreateFramebuffer(dev->LogicalDev(), &fbCI, nullptr, &fb));

        auto scfb = new _SCFB(dev);
        scfb->_sc = sc;
        //scfb->_fb = fb;
        scfb->_colorTarget = colorTarget;
        scfb->_depthTarget = depthTarget;
        scfb->_colorTargetView = attachments[0];
        scfb->_depthTargetView = attachments[1];
        scfb->_fbDesc.colorTargets = { {colorTarget, colorTarget->GetDesc().arrayLayers, colorTarget->GetDesc().mipLevels} };
        if(depthTarget)
            scfb->_fbDesc.depthTarget = {depthTarget, depthTarget->GetDesc().arrayLayers, depthTarget->GetDesc().mipLevels};
            //VulkanFrameBufferBase::CreateCompatibleRenderPasses(_gd, _fbDesc, true,
            

        return common::sp(scfb);
    }

#endif
    void VulkanSwapChain::ReleaseFramebuffers(){
        //vkDestroyRenderPass(_gd->LogicalDev(), _renderPassNoClear, nullptr);
        //vkDestroyRenderPass(_gd->LogicalDev(), _renderPassNoClearLoad, nullptr);
        //vkDestroyRenderPass(_gd->LogicalDev(), _renderPassClear, nullptr);

#ifdef VLD_DEBUG
        //for(auto& fb : _fbs){
        //    //there shouldn't have outside references
        //    assert(fb->unique());
        //}
#endif

        _fbs.clear();

#ifdef VLD_DEBUG
        //if(_depthTarget != nullptr){
        //    assert(_depthTarget->unique());
        //}
#endif

        _depthTarget = nullptr;
    }

    void VulkanSwapChain::CreateFramebuffers(){
        if(_scExtent.width == 0 || _scExtent.height == 0) return;

        //Acquire images -> create image views -> wrap inside framebuffers

        // Get the images
        std::uint32_t scImageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(_dev->LogicalDev(), _deviceSwapchain, &scImageCount, nullptr));
        std::vector<VkImage> scImgs(scImageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(_dev->LogicalDev(), _deviceSwapchain, &scImageCount, scImgs.data()));

        assert(scImageCount > 0);
        auto _CreateRT = [&](
            const common::sp<VulkanTexture>& colorTgt, 
            const common::sp<VulkanTexture>& dsTgt
        ) {
            this->ref();
            auto spsc = common::sp(this);
            BackBufferContainer fb { };

            ITextureView::Description ctvDesc {};
            ctvDesc.mipLevels = 1;
            ctvDesc.arrayLayers = 1;
            auto ctView = VulkanTextureView::Make(colorTgt,ctvDesc);
            fb.colorTgt = ctView;

            if(dsTgt) {
                ITextureView::Description dsvDesc {};
                dsvDesc.mipLevels = 1;
                dsvDesc.arrayLayers = 1;
                auto dsView = VulkanTextureView::Make(dsTgt,dsvDesc);
                fb.dsTgt = dsView;
            }

            _fbs.push_back(std::move(fb));

            //auto fb = _SCFB::Make(_dev, spsc, colorTgt, dsTgt);
            //assert(fb != nullptr);
        };
#if 0
        //_scExtent = swapchainExtent;
        //Framebuffer description Prepare render passes
        //CreateDepthTexture();
        if (description.depthFormat.has_value()) {
            Texture::Description texDesc{};
            texDesc.type = alloy::Texture::Description::Type::Texture2D;
            texDesc.width = _scExtent.width;
            texDesc.height = _scExtent.height;
            texDesc.depth = 1;
            texDesc.mipLevels = 1;
            texDesc.arrayLayers = 1;
            texDesc.usage.depthStencil = true;
            texDesc.sampleCount = SampleCount::x1;
            texDesc.format = description.depthFormat.value();

            auto dTgt = _gd->GetResourceFactory()
                ->CreateTexture(texDesc);
            auto vkDTgt = PtrCast<VulkanTexture>(dTgt.get());
            vkDTgt->SetLayout(VK_IMAGE_LAYOUT_UNDEFINED);

            _depthTarget = RefRawPtr(vkDTgt);
            _fbDesc.depthTarget = { _depthTarget, _depthTarget->GetDesc().arrayLayers, _depthTarget->GetDesc().mipLevels };
        }

        //Texture::Description texDesc{};
        {
            auto firstTex = scImgs.front();
            texDesc.type = alloy::Texture::Description::Type::Texture2D;
            texDesc.width = _scExtent.width;
            texDesc.height = _scExtent.height;
            texDesc.depth = 1;
            texDesc.mipLevels = 1;
            texDesc.arrayLayers = 1;
            texDesc.usage.renderTarget = true;
            texDesc.sampleCount = SampleCount::x1;
            texDesc.format = alloy::VK::priv::VkToVdPixelFormat(_surfaceFormat.format);

            auto firstColorTgt = alloy::VulkanTexture::WrapNative(RefRawPtr(_gd), texDesc,
                VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, firstTex
            );

            //auto firstColorTgt = _gd->GetResourceFactory()->WrapNativeTexture(firstTex, texDesc);
            auto vkFirstColorTgt = PtrCast<VulkanTexture>(firstColorTgt.get());
            _fbDesc.colorTargets = { {firstColorTgt, firstColorTgt->GetDesc().arrayLayers, firstColorTgt->GetDesc().mipLevels} };
            //VulkanFrameBufferBase::CreateCompatibleRenderPasses(_gd, _fbDesc, true,
            //    _renderPassNoClear, _renderPassNoClearLoad, _renderPassClear);

            _CreateSCFB(vkFirstColorTgt);
        }
#endif

        ITexture::Description colorDesc{}, dsDesc{};

        if (description.depthFormat.has_value()) {
            dsDesc.type = ITexture::Description::Type::Texture2D;
            dsDesc.width = _scExtent.width;
            dsDesc.height = _scExtent.height;
            dsDesc.depth = 1;
            dsDesc.mipLevels = 1;
            dsDesc.arrayLayers = 1;
            dsDesc.usage.depthStencil = true;
            dsDesc.sampleCount = SampleCount::x1;
            dsDesc.format = description.depthFormat.value();
        }

        {
            colorDesc.type = ITexture::Description::Type::Texture2D;
            colorDesc.width = _scExtent.width;
            colorDesc.height = _scExtent.height;
            colorDesc.depth = 1;
            colorDesc.mipLevels = 1;
            colorDesc.arrayLayers = 1;
            colorDesc.usage.renderTarget = true;
            colorDesc.sampleCount = SampleCount::x1;
            colorDesc.format = VkToVdPixelFormat(_surfaceFormat.format);
        }



        for (unsigned i = 0; i < scImgs.size(); i++) {
            auto tex = scImgs[i];
            auto colorTgt = VulkanTexture::WrapNative(_dev, colorDesc,
                VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tex
            );

            auto colorTex = SPCast<VulkanTexture>(colorTgt);
            common::sp<VulkanTexture> dsTex {};

            //Create depth stencil target if requested
            if (description.depthFormat.has_value()) {
                
                auto dTgt = _dev->GetResourceFactory()
                    .CreateTexture(dsDesc);
                dsTex = SPCast<VulkanTexture>(dTgt);
                dsTex->SetLayout(VK_IMAGE_LAYOUT_UNDEFINED);
                //_fbDesc.depthTarget = { _depthTarget, _depthTarget->GetDesc().arrayLayers, _depthTarget->GetDesc().mipLevels };
            }


            _CreateRT(colorTex, dsTex);
        }

        
        //CreateFramebuffers();
        //for(auto& tex : scImgs){
        //    Texture::Description texDesc{};
        //    texDesc.type = alloy::Texture::Description::Type::Texture2D;
        //    texDesc.width = _scExtent.width;
        //    texDesc.height = _scExtent.height;
        //    texDesc.depth = 1;
        //    texDesc.mipLevels = 1;
        //    texDesc.arrayLayers = 1;
        //    texDesc.usage.renderTarget = true;
        //    texDesc.sampleCount = Texture::Description::SampleCount::x1;
        //    texDesc.format = VkToVdPixelFormat(_surfaceFormat.format);
        //
        //    auto colorTgt = _gd->GetResourceFactory()->WrapNativeTexture(tex, texDesc);
        //    auto vkColorTgt = PtrCast<VulkanTexture>(colorTgt.get());
        //
        //    this->ref();
        //    auto spsc = sp(this);
        //    auto fb = _SCFB::Make(_gd, spsc, RefRawPtr(vkColorTgt), _depthTarget);
        //    assert(fb != nullptr);
        //    _fbs.push_back(std::move(fb));
        //}
        
    }

    VkResult VulkanSwapChain::AcquireNextImage(VkSemaphore semaphore, VkFence fence)
    {

        if (_newSyncToVBlank != _syncToVBlank)
        {
            _syncToVBlank = _newSyncToVBlank;
            CreateSwapchain(GetWidth(), GetHeight());
            //return VK_SUCCESS;//TODO: really success?
        }

        VkResult result = vkAcquireNextImageKHR(
            _dev->LogicalDev(),
            _deviceSwapchain,
            UINT64_MAX,
            semaphore,
            fence,
            &_currentImageIndex);
        //_framebuffer.SetImageIndex(_currentImageIndex);
        //if (    result == VkResult::VK_ERROR_OUT_OF_DATE_KHR 
        //        || result == VkResult::VK_SUBOPTIMAL_KHR        )
        //{
        //    CreateSwapchain(GetWidth(), GetHeight());
        //    return false;
        //}
        //
        if (result != VkResult::VK_SUCCESS)
        {
            //throw new alloyException("Could not acquire next image from the Vulkan swapchain.");
            //assert(false);
            return result;
        }
        //_fbs[_currentImageIndex]->SetToInitialLayout();
        return VK_SUCCESS;
    }

    //void VulkanSwapChain::RecreateAndReacquire(std::uint32_t width, std::uint32_t height)
    //{
    //    auto _gd = PtrCast<VulkanDevice>(dev.get());
    //    if (CreateSwapchain(width, height))
    //    {
    //        auto res = AcquireNextImage(VK_NULL_HANDLE, _imageAvailableFence);
    //        if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR)
    //        {
    //            vkWaitForFences(_gd->LogicalDev(), 1, &_imageAvailableFence, true, UINT64_MAX);
    //            vkResetFences(_gd->LogicalDev(), 1, &_imageAvailableFence);
    //        }
    //    }
    //}


    bool VulkanSwapChain::CreateSwapchain(std::uint32_t width, std::uint32_t height)
    {
        ReleaseFramebuffers();

        auto surface = _surf.surface;
        if(surface == VK_NULL_HANDLE){
            return false;
        }
        // Obtain the surface capabilities first -- this will indicate whether the surface has been lost.
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _dev->GetPhyDevInfo().handle, 
            surface, 
            &surfaceCapabilities);

        if (result == VkResult::VK_ERROR_SURFACE_LOST_KHR)
        {
            //throw new alloyException($"The Swapchain's underlying surface has been lost.");
            return false;
        }

        if (   surfaceCapabilities.minImageExtent.width  == 0 
            && surfaceCapabilities.minImageExtent.height == 0
            && surfaceCapabilities.maxImageExtent.width  == 0
            && surfaceCapabilities.maxImageExtent.height == 0
        ){
            return false;
        }

        if (_deviceSwapchain != VK_NULL_HANDLE)
        {
            _dev->WaitForIdle();
        }

        _currentImageIndex = 0;
        std::uint32_t surfaceFormatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(_dev->PhysicalDev(), surface, &surfaceFormatCount, nullptr));
        std::vector<VkSurfaceFormatKHR> formats (surfaceFormatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(_dev->PhysicalDev(), surface, &surfaceFormatCount, formats.data()));

        VkFormat desiredFormat = description.colorSrgb
            ? VkFormat::VK_FORMAT_B8G8R8A8_SRGB
            : VkFormat::VK_FORMAT_B8G8R8A8_UNORM;

        //Hopefully default value is vk_format_undefined
        _surfaceFormat = {};
        if (formats.size() == 1 && formats.front().format == VkFormat::VK_FORMAT_UNDEFINED) {
            _surfaceFormat.colorSpace = VkColorSpaceKHR::VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            _surfaceFormat.format = desiredFormat;
        }
        else {
            //search for a non-linear color space?
            for (VkSurfaceFormatKHR& format : formats) {
                if (   format.colorSpace == VkColorSpaceKHR::VK_COLORSPACE_SRGB_NONLINEAR_KHR 
                    && format.format     == desiredFormat
                ){
                    _surfaceFormat = format;
                    break;
                }
            }
            if (_surfaceFormat.format == VkFormat::VK_FORMAT_UNDEFINED)
            {
                assert(!description.colorSrgb);
                {
                    //throw new alloyException($"Unable to create an sRGB Swapchain for this surface.");
                }

                _surfaceFormat = formats[0];
            }
        }

        std::uint32_t presentModeCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(_dev->PhysicalDev(), surface, &presentModeCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(_dev->PhysicalDev(), surface, &presentModeCount, presentModes.data()));
        
        VkPresentModeKHR presentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;

        auto _FindAndSetMode = [&](VkPresentModeKHR mode)->bool{
            for(auto& m : presentModes) {
                if (m == mode) {
                    presentMode = mode;
                    return true;
                }
            }
            return false;
        };

        if (this->IsSyncToVerticalBlank()) {
            _FindAndSetMode(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_RELAXED_KHR);
        }
        else {
            //Prefer mailbox mode
            bool succeeded =  _FindAndSetMode(VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR);
            if(!succeeded){
                succeeded =  _FindAndSetMode(VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR);
            }
        }

        std::uint32_t maxImageCount 
            = surfaceCapabilities.maxImageCount == 0 
                ? UINT32_MAX
                : surfaceCapabilities.maxImageCount;
        std::uint32_t minImageCount = surfaceCapabilities.minImageCount;
        //std::uint32_t imageCount = std::min(maxImageCount, surfaceCapabilities.minImageCount + 1);

        std::uint32_t imageCount = description.backBufferCnt;
        if(imageCount < minImageCount) imageCount = minImageCount;
        else if(imageCount > maxImageCount) imageCount = maxImageCount;

        VkSwapchainCreateInfoKHR swapchainCI{};
        swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCI.surface = surface;
        swapchainCI.presentMode = presentMode;
        swapchainCI.imageFormat = _surfaceFormat.format;
        swapchainCI.imageColorSpace = _surfaceFormat.colorSpace;
        std::uint32_t clampedWidth = std::clamp(width,
            surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        std::uint32_t clampedHeight = std::clamp(height,
            surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
        swapchainCI.imageExtent = VkExtent2D { width = clampedWidth, height = clampedHeight };
        swapchainCI.minImageCount = imageCount;
        swapchainCI.imageArrayLayers = 1;
        swapchainCI.imageUsage 
            = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;


        //TODO: add support to differnt gfx queue and present queue.
        //std::uint32_t queueFamilyIndices[] {_gd.GraphicsQueueIndex, _gd.PresentQueueIndex};
        //if (_gd.GraphicsQueueIndex != _gd.PresentQueueIndex)
        //{
        //    swapchainCI.imageSharingMode = VkSharingMode::VK_SHARING_MODE_CONCURRENT;
        //    swapchainCI.queueFamilyIndexCount = 2;
        //    swapchainCI.pQueueFamilyIndices = queueFamilyIndices;
        //}
        //else
        {
            swapchainCI.imageSharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
            swapchainCI.queueFamilyIndexCount = 0;
        }

        swapchainCI.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainCI.compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCI.clipped = true;

        VkSwapchainKHR oldSwapchain = _deviceSwapchain;
        swapchainCI.oldSwapchain = oldSwapchain;

        VK_CHECK(vkCreateSwapchainKHR(_dev->LogicalDev(), &swapchainCI, nullptr, &_deviceSwapchain));

        if (oldSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(_dev->LogicalDev(), oldSwapchain, nullptr);
        }

        _scExtent = {width, height};
        CreateFramebuffers();
        //_framebuffer.SetNewSwapchain(_deviceSwapchain, width, height, surfaceFormat, swapchainCI.imageExtent);
        return true;
    }


    VulkanSwapChain::~VulkanSwapChain(){
        ReleaseFramebuffers();

        vkDestroyFence(_dev->LogicalDev(), _imageAvailableFence, nullptr);
        //_framebuffer.Dispose();
        vkDestroySwapchainKHR(_dev->LogicalDev(), _deviceSwapchain, nullptr);

        if(_surf.isOwnSurface){
            vkDestroySurfaceKHR(_dev->GetInstance(), _surf.surface, nullptr);
        }
    }

    common::sp<ISwapChain> VulkanSwapChain::Make(
        const common::sp<VulkanDevice>& dev,
        const ISwapChain::Description& desc
    ){
        auto _syncToVBlank = desc.syncToVerticalBlank;
        auto* _swapchainSource = desc.source;
        auto _colorSrgb = desc.colorSrgb;

        //Make the surface if possible
        VK::priv::SurfaceContainer _surf = {VK_NULL_HANDLE, false};

        if (_swapchainSource != nullptr) {
            _surf = VK::priv::CreateSurface(dev->GetInstance(), _swapchainSource);
        }

        //auto _surface = dev->Surface();

        if (_surf.surface == VK_NULL_HANDLE) return nullptr;

        //if (existingSurface == nullptr)
        //{
        //    _surface = CreateSurface(gd.Instance, _swapchainSource);
        //}
        //else
        //{
        //    _surface = existingSurface;
        //}

        //if (!GetPresentQueueIndex(out _presentQueueIndex))
        //{
        //    throw new alloyException($"The system does not support presenting the given Vulkan surface.");
        //}
        //vkGetDeviceQueue(_gd.Device, _presentQueueIndex, 0, out _presentQueue);

        auto sc = new VulkanSwapChain(dev, desc);
        sc->_surf = _surf;
        //sc->_renderPassClear = VK_NULL_HANDLE;
        //sc->_renderPassNoClear = VK_NULL_HANDLE;
        //sc->_renderPassNoClearLoad = VK_NULL_HANDLE;
        sc->_deviceSwapchain = VK_NULL_HANDLE;


        //_framebuffer = new VkSwapchainFramebuffer(gd, this, _surface, description.Width, description.Height, description.DepthFormat);


        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = 0;

        VkFence _imageAvailableFence;
        VK_CHECK(vkCreateFence(dev->LogicalDev(), &fenceCI, nullptr, &sc->_imageAvailableFence));

        sc->CreateSwapchain(desc.initialWidth, desc.initialHeight);
        //AcquireNextImage(_gd.Device, VkSemaphore.Null, _imageAvailableFence);
        //vkWaitForFences(_gd.Device, 1, ref _imageAvailableFence, true, ulong.MaxValue);
        //vkResetFences(_gd.Device, 1, ref _imageAvailableFence);

        return common::sp(sc);

    }

    common::sp<IFrameBuffer> VulkanSwapChain::GetBackBuffer() {

        //Acquire next frame and wait for ready fence

        VkResult res = VK_SUCCESS;

        if(_currentImageInUse) {
            auto res = AcquireNextImage(VK_NULL_HANDLE, _imageAvailableFence);

            if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR)
            {
                vkWaitForFences(_dev->LogicalDev(), 1, &_imageAvailableFence, true, UINT64_MAX);
                vkResetFences(_dev->LogicalDev(), 1, &_imageAvailableFence);
                res = VK_SUCCESS;
                _currentImageInUse = false;
            }
        }
        if (res == VK_SUCCESS ){
            //Swapchain image may be 0 when app minimized
            if (_currentImageIndex < _fbs.size()) {
                ref();
                return common::sp(
                    new _SCFB(_dev, common::sp(this), _fbs[_currentImageIndex])
                );
            }
            else 
                return nullptr;
        } else {
            return nullptr;
        }
        ///#TODO: handle case VK_ERROR_OUT_OF_DATE_KHR
    }

    //Transition color targets from VK_LAYOUT_UNDEFINED to VK_LAYOUT_GENERAL
    //transition depth stencil targets from VK_LAYOUT_PREINITIALIZED to VK_LAYOUT_GENERAL
    //void VulkanSwapChain::PerformInitialLayoutTransition() {
    //
    //
    //    auto _gd = PtrCast<VulkanDevice>(dev.get());
    //
    //    //Allocate command buffer
    //
    //
    //    //Gather and build transition command
    //
    //    //submit on default graphics queue
    //
    //    //wait for fence
    //
    //    vkWaitForFences(_gd->LogicalDev(), 1, &_imageAvailableFence, true, UINT64_MAX);
    //    vkResetFences(_gd->LogicalDev(), 1, &_imageAvailableFence);
    //}

} // namespace alloy
