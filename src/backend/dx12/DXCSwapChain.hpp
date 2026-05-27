#pragma once

//3rd-party headers

//alloy public headers
#include "alloy/common/Common.hpp"
#include "alloy/common/RefCnt.hpp"
#include "alloy/Helpers.hpp"
#include "alloy/SwapChain.hpp"

//standard library headers
#include <string>
#include <vector>

//backend specific headers
#include "DXCTexture.hpp"

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers



namespace alloy::dxc {

    class DXCDevice;
    class DXCSwapChain;
    //Meant for quickly create, use, then dispose. Provide a 
    // strong reference to swapchain, otherwise is an ordinary
    // texture view.
    class DXCSwapChainBackBufferView : public DXCTextureView {
        common::sp<DXCSwapChain> _sc;

    public:

        DXCSwapChainBackBufferView(
            common::sp<DXCSwapChain> sc,
            common::sp<DXCTexture> target,
            const ITextureView::Description& desc
        ) 
            : DXCTextureView(std::move(target), desc)
            , _sc(std::move(sc))
        { }
    
        virtual ~DXCSwapChainBackBufferView() override;
    };
    
    class DXCSwapChain : public ISwapChain {
        friend class DXCSwapChainBackBuffer;
        friend class DXCSwapChainRenderTarget;

        common::sp<DXCDevice> _dev;

        IDXGISwapChain3* _sc;

        //ID3D12DescriptorHeap* _rtvHeap;
        //ID3D12DescriptorHeap* _dsvHeap;
           
        //sp<VulkanTexture> _depthTarget;
        //std::vector<BackBufferContainer> _fbs;
        //uint32_t _fbCnt;
        std::vector<common::sp<DXCTexture>> _backBuffers;
        uint32_t _backBufferCnt;

        //VkSwapchainKHR _deviceSwapchain;
        //VkExtent2D _scExtent;
        //VkSurfaceFormatKHR _surfaceFormat;
        bool _syncToVBlank, _newSyncToVBlank;
        bool _srgbColorSpace;

        //VkFence _imageAvailableFence;
        //std::uint32_t _currentImageIndex;
        Description _desc;

        DXCSwapChain(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        ) : _dev(dev)
          , _srgbColorSpace(desc.colorSrgb)
          , _desc(desc)
        {
            _syncToVBlank = _newSyncToVBlank = desc.syncToVerticalBlank;
        }

        bool CreateSwapchain(std::uint32_t width, std::uint32_t height);

        void ReleaseFramebuffers();
        void CreateFramebuffers(std::uint32_t width, std::uint32_t height);

        static DXGI_FORMAT GetDesiredPixelFormat(bool srgb) {
            return srgb? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
                       : DXGI_FORMAT_B8G8R8A8_UNORM
                       ;
        }
        
        //void SetImageIndex(std::uint32_t index) {_currentImageIndex = index; }

        //VkResult AcquireNextImage(VkSemaphore semaphore, VkFence fence);

        //void RecreateAndReacquire(std::uint32_t width, std::uint32_t height);
        

    public:

        ~DXCSwapChain() override;

        static common::sp<ISwapChain> Make(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        );

        IDXGISwapChain3* GetHandle() const { return _sc; }

        std::uint32_t GetCurrentImageIdx() const { return _sc->GetCurrentBackBufferIndex(); }
        DXCTexture* GetCurrentColorTarget() const { return _backBuffers[GetCurrentImageIdx()].get(); }

    public:

        const Description& GetDesc() const override { return _desc; }

        common::sp<ITextureView> GetBackBuffer() override;

        virtual uint32_t GetBackBufferIndex() override { return GetCurrentImageIdx(); }

        void Resize(
            std::uint32_t width, 
            std::uint32_t height) override;
        //{
        //    CreateSwapchain(width, height);
        //    //RecreateAndReacquire(width, height);
        //}

        bool IsSyncToVerticalBlank() const override {return _syncToVBlank;}
        void SetSyncToVerticalBlank(bool sync) override {_newSyncToVBlank = sync;}

        std::uint32_t GetWidth() const override ;
        std::uint32_t GetHeight() const override ;

        //virtual State SwapBuffers() override;
    };

}
