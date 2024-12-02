#pragma once

//3rd-party headers

//veldrid public headers
#include "veldrid/common/Common.hpp"
#include "veldrid/common/RefCnt.hpp"
#include "veldrid/Helpers.hpp"
#include "veldrid/SwapChain.hpp"
#include "veldrid/Framebuffer.hpp"

//standard library headers
#include <string>
#include <vector>

//backend specific headers
#include "DXCTexture.hpp"

//platform specific headers
#include <directx/d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers



namespace Veldrid {

    class DXCDevice;
    class DXCSwapChain;

    struct BackBufferContainer {
        //DXCSwapChain* _sc;
        sp<Texture> _colorBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE _rtv;
        
        sp<Texture> _depthBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE _dsv;

        Framebuffer::Description _fbDesc;
    };

    class DXCSwapChainBackBuffer : public Framebuffer {
       
        sp<DXCSwapChain> _sc;
        const BackBufferContainer& _bb;

    public:
        DXCSwapChainBackBuffer(
            const sp<GraphicsDevice>& dev,
            sp<DXCSwapChain>&& sc,
            const BackBufferContainer& bb
        );

        ~DXCSwapChainBackBuffer() override;

        
        virtual const Description& GetDesc() const override {return _bb._fbDesc;}

    };
    
    class DXCSwapChain : public SwapChain {
        friend class DXCSwapChainBackBuffer;

        IDXGISwapChain3* _sc;

        ID3D12DescriptorHeap* _rtvHeap;
        ID3D12DescriptorHeap* _dsvHeap;
           
        //sp<VulkanTexture> _depthTarget;
        std::vector<BackBufferContainer> _fbs;

        //VkSwapchainKHR _deviceSwapchain;
        //VkExtent2D _scExtent;
        //VkSurfaceFormatKHR _surfaceFormat;
        bool _syncToVBlank, _newSyncToVBlank;

        //VkFence _imageAvailableFence;
        std::uint32_t _currentImageIndex;

        DXCSwapChain(
            const sp<GraphicsDevice>& dev,
            const Description& desc
            ) : SwapChain(dev, desc){
            _syncToVBlank = _newSyncToVBlank = desc.syncToVerticalBlank;
        }

        bool CreateSwapchain(std::uint32_t width, std::uint32_t height);

        void ReleaseFramebuffers();
        void CreateFramebuffers();

        void SetImageIndex(std::uint32_t index) {_currentImageIndex = index; }

        //VkResult AcquireNextImage(VkSemaphore semaphore, VkFence fence);

        //void RecreateAndReacquire(std::uint32_t width, std::uint32_t height);
        

    public:

        ~DXCSwapChain() override;

        static sp<SwapChain> Make(
            const sp<DXCDevice>& dev,
            const Description& desc
        );

        IDXGISwapChain3* GetHandle() const { return _sc; }

        std::uint32_t GetCurrentImageIdx() const { return _sc->GetCurrentBackBufferIndex(); }

    public:
        sp<Framebuffer> GetFramebuffer() override;

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

        virtual State SwitchToNextFrameBuffer(
            //Semaphore* signalSemaphore,
            //Fence* fence
        ) override;
    };




}
