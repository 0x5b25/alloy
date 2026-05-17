#pragma once

//3rd-party headers

//alloy public headers
#include "alloy/common/Common.hpp"
#include "alloy/common/RefCnt.hpp"
#include "alloy/Helpers.hpp"
#include "alloy/SwapChain.hpp"
#include "alloy/Framebuffer.hpp"

//standard library headers
#include <string>
#include <vector>

//backend specific headers
#include "DXCTexture.hpp"
#include "DXCFrameBuffer.hpp"

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers



namespace alloy::dxc {

    class DXCDevice;
    class DXCSwapChain;

    struct RenderTargetContainer {
        common::sp<DXCTextureView> tex;
        D3D12_CPU_DESCRIPTOR_HANDLE view;
    };
    struct BackBufferContainer {
        //DXCSwapChain* _sc;
        RenderTargetContainer colorTgt;
        
        RenderTargetContainer dsTgt;

        //IFrameBuffer::Description fbDesc;
    };

    class DXCSwapChainRenderTarget : public DXCRenderTargetBase {
        common::sp<DXCSwapChain> _sc;
        const RenderTargetContainer& _rt;

        int generation;
        
    public:

        DXCSwapChainRenderTarget(
            const common::sp<DXCSwapChain>& sc,
            const RenderTargetContainer& rt,
            int gen
        )
            : _sc(sc)
            , _rt(rt)
            , generation(gen)
        { }
        
        virtual ITextureView& GetTexture() const override;

        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const override;
    };

    class DXCSwapChainBackBuffer : public IFrameBuffer {
       
        common::sp<DXCSwapChain> _sc;
        const BackBufferContainer& _bb;

        int generation;

    public:
        DXCSwapChainBackBuffer(
            common::sp<DXCSwapChain>&& sc,
            const BackBufferContainer& bb,
            int gen
        );

        ~DXCSwapChainBackBuffer() override;
        
        virtual OutputDescription GetDesc() override;

        uint32_t GetRTVCount() const;
        bool HasDSV() const;
        bool DSVHasStencil() const;
        
        D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(uint32_t slot) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;

    };
    
    class DXCSwapChain : public ISwapChain {
        friend class DXCSwapChainBackBuffer;
        friend class DXCSwapChainRenderTarget;

        common::sp<DXCDevice> _dev;

        IDXGISwapChain3* _sc;

        ID3D12DescriptorHeap* _rtvHeap;
        ID3D12DescriptorHeap* _dsvHeap;
           
        //sp<VulkanTexture> _depthTarget;
        std::vector<BackBufferContainer> _fbs;
        uint32_t _fbCnt;

        //VkSwapchainKHR _deviceSwapchain;
        //VkExtent2D _scExtent;
        //VkSurfaceFormatKHR _surfaceFormat;
        bool _syncToVBlank, _newSyncToVBlank;
        bool _srgbColorSpace;

        //VkFence _imageAvailableFence;
        //std::uint32_t _currentImageIndex;

        int generation;

        DXCSwapChain(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        ) : ISwapChain(desc)
          , _dev(dev)
          , _srgbColorSpace(desc.colorSrgb)
          , generation(0)
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
        DXCTextureView* GetCurrentColorTarget() const { return _fbs[GetCurrentImageIdx()].colorTgt.tex.get(); }

    public:
        common::sp<IFrameBuffer> GetBackBuffer() override;

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

        bool HasDepthTarget() const {return description.depthFormat.has_value();}

        std::uint32_t GetWidth() const override ;
        std::uint32_t GetHeight() const override ;

        //virtual State SwapBuffers() override;
    };

}
