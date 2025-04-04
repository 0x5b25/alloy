#include "DXCSwapChain.hpp"

#include "D3DCommon.hpp"
#include "DXCDevice.hpp"
#include "DXCTexture.hpp"
#include "D3DTypeCvt.hpp"
#include "DXCContext.hpp"

namespace alloy::dxc {


    DXCSwapChainBackBuffer::DXCSwapChainBackBuffer(
        common::sp<DXCSwapChain>&& sc,
        const BackBufferContainer& bb
    )
        : _sc(std::move(sc))
        , _bb(bb)
    {}

    
    DXCSwapChainBackBuffer::~DXCSwapChainBackBuffer() {  }

    uint32_t DXCSwapChainBackBuffer::GetRTVCount() const { return 1; }

    bool DXCSwapChainBackBuffer::HasDSV() const { return _bb.dsTgt.tex != nullptr; }

    
    bool DXCSwapChainBackBuffer::DSVHasStencil() const {
        if(_bb.dsTgt.tex) {
            auto& dxcTex = _bb.dsTgt.tex->GetTextureObject();
            if(FormatHelpers::IsStencilFormat(
                dxcTex->GetDesc().format
            )) return true;
        }
        
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DXCSwapChainBackBuffer::GetRTV(uint32_t slot) const {
        return _bb.colorTgt.view;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DXCSwapChainBackBuffer::GetDSV() const {
        return _bb.dsTgt.view;
    }

    
    OutputDescription DXCSwapChainBackBuffer::GetDesc() {
        OutputDescription desc { };
        desc.colorAttachments.push_back(common::sp(new DXCSwapChainRenderTarget(
            _sc,
            _bb.colorTgt
        )));

        if(HasDSV()) {
            desc.depthAttachment.reset(new DXCSwapChainRenderTarget(
                _sc,
                _bb.dsTgt
            ));
        }

        desc.sampleCount = _bb.colorTgt.tex->GetTextureObject()->GetDesc().sampleCount;

        return desc;
    }
    
    common::sp<ISwapChain> DXCSwapChain::Make(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    ) {
        //IDXGIDevice3 * pDXGIDevice = nullptr;
        //auto hr = dev->GetDevice()->QueryInterface(__uuidof(IDXGIDevice3), (void **)&pDXGIDevice);

        auto& dxcAdp = static_cast<DXCAdapter&>(dev->GetAdapter());
        IDXGIAdapter1* pDXGIAdapter = dxcAdp.GetHandle();

        IDXGIFactory4* pIDXGIFactory = nullptr;
        pDXGIAdapter->GetParent(__uuidof(IDXGIFactory4), (void **)&pIDXGIFactory);

        assert(desc.source->tag == SwapChainSource::Tag::Win32);
        auto swapChainSrc = (const Win32SwapChainSource*)desc.source;

        auto vldSC = common::sp(new DXCSwapChain(dev, desc));

        ///#TODO: settle for now. Add to swapchain desc later maybe
        uint32_t backBufferCnt = desc.backBufferCnt;

        // Describe and create the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = backBufferCnt; 
        swapChainDesc.Width = desc.initialWidth;
        swapChainDesc.Height = desc.initialHeight;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; ///#TODO: add format support
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        IDXGISwapChain1* swapChain;
        pIDXGIFactory->CreateSwapChainForHwnd(
            dev->GetImplicitQueue(),   // Swap chain needs the queue so that it can force a flush on it.
            (HWND)swapChainSrc->hWnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain
            );

        pIDXGIFactory->Release();

        
        IDXGISwapChain3* dxcSwapChain;
        swapChain->QueryInterface(&dxcSwapChain);
        swapChain->Release();

        vldSC->_sc = dxcSwapChain;
        vldSC->_fbCnt = backBufferCnt;

        vldSC->CreateFramebuffers(desc.initialWidth, desc.initialHeight);

        ///#TODO: support MSAA
                

        //// Create a RTV for each frame.
        //ID3D12DescriptorHeap* pRtvHeap = nullptr;
        //
        //D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        //heapDesc.NumDescriptors = backBufferCnt;
        //heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        //ThrowIfFailed(dev->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pRtvHeap)));
        //vldSC->_rtvHeap = pRtvHeap;
        //
        //auto rtvHandle = pRtvHeap->GetCPUDescriptorHandleForHeapStart();
        //auto rtvDescriptorSize = dev->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        //
        //ID3D12DescriptorHeap* pDsvHeap = nullptr;
        //D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle {};
        //uint32_t dsvDescriptorSize = 0;
        //
        //if(desc.depthFormat.has_value()) {
        //    D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        //    heapDesc.NumDescriptors = backBufferCnt;
        //    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        //    ThrowIfFailed(dev->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pDsvHeap)));
        //    dsvHandle = pDsvHeap->GetCPUDescriptorHandleForHeapStart();
        //    dsvDescriptorSize = dev->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        //    
        //}
        //vldSC->_dsvHeap = pDsvHeap;
        //
        //
        //
        //Texture::Description texDesc{};  
        //texDesc.type = alloy::Texture::Description::Type::Texture2D;
        //texDesc.width = desc.initialWidth;
        //texDesc.height = desc.initialHeight;
        //texDesc.depth = 1;
        //texDesc.mipLevels = 1;
        //texDesc.arrayLayers = 1;
        //texDesc.usage.renderTarget = true;
        //texDesc.sampleCount = SampleCount::x1;
        //texDesc.format = D3DToVdPixelFormat(swapChainDesc.Format);
        //    
        //
        //std::vector<BackBufferContainer> backBuffers;
        //for (uint32_t n = 0; n < backBufferCnt; n++)
        //{
        //    ID3D12Resource* pTex;
        //    ThrowIfFailed(dxcSwapChain->GetBuffer(n, IID_PPV_ARGS(&pTex)));
        //    dev->GetDevice()->CreateRenderTargetView(pTex, nullptr, rtvHandle);
        //
        //    auto vldTex = DXCTexture::WrapNative(dev, texDesc, pTex);
        //
        //    Framebuffer::Description fbDesc;
        //    fbDesc.colorTargets = { 
        //        {vldTex, vldTex->GetDesc().arrayLayers, vldTex->GetDesc().mipLevels}
        //    };
        //
        //    sp<Texture> vldDepthTex;
        //    if (desc.depthFormat.has_value()) {
        //
        //        Texture::Description dsDesc{};
        //        dsDesc.type = alloy::Texture::Description::Type::Texture2D;
        //        dsDesc.width = desc.initialWidth;
        //        dsDesc.height = desc.initialHeight;
        //        dsDesc.depth = 1;
        //        dsDesc.mipLevels = 1;
        //        dsDesc.arrayLayers = 1;
        //        dsDesc.usage.depthStencil = true;
        //        dsDesc.sampleCount = SampleCount::x1;
        //        dsDesc.format = desc.depthFormat.value();
        //
        //        vldDepthTex = dev->GetResourceFactory()->CreateTexture(dsDesc);
        //
        //        fbDesc.depthTarget = { vldDepthTex, vldDepthTex->GetDesc().arrayLayers, vldDepthTex->GetDesc().mipLevels };
        //
        //        auto dxcDepthTex = PtrCast<DXCTexture>(vldDepthTex.get());
        //        dev->GetDevice()->CreateDepthStencilView(dxcDepthTex->GetHandle(), nullptr, dsvHandle);
        //
        //    }
        //
        //    backBuffers.emplace_back(vldTex, rtvHandle, vldDepthTex, dsvHandle, fbDesc);
        //
        //    rtvHandle.ptr += rtvDescriptorSize;
        //    dsvHandle.ptr += dsvDescriptorSize;
        //}
        //
        //vldSC->_fbs = std::move(backBuffers);

        return vldSC;
    }

    
    DXCSwapChain::~DXCSwapChain() {
        ReleaseFramebuffers();
        _sc->Release();
    }

    common::sp<IFrameBuffer> DXCSwapChain::GetBackBuffer() {
        //Swapchain image may be 0 when app minimized
        auto nextFrameIdx = GetCurrentImageIdx();
        if (nextFrameIdx >= _fbs.size()) return nullptr;

        this->ref();

        return common::sp(new DXCSwapChainBackBuffer(common::sp(this), _fbs[nextFrameIdx]));
    }

    void DXCSwapChain::Resize(
        std::uint32_t width, 
        std::uint32_t height
    ) {
        ReleaseFramebuffers();
        _sc->ResizeBuffers(0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        CreateFramebuffers(width, height);
    }

    
    std::uint32_t DXCSwapChain::GetWidth() const {
        DXGI_SWAP_CHAIN_DESC desc{};
        _sc->GetDesc(&desc);
        return desc.BufferDesc.Width;
    }
    std::uint32_t DXCSwapChain::GetHeight() const {
        DXGI_SWAP_CHAIN_DESC desc{};
        _sc->GetDesc(&desc);
        return desc.BufferDesc.Height;
    }

    
    void DXCSwapChain::ReleaseFramebuffers() {
        _fbs.clear();
        _rtvHeap->Release();
        if(_dsvHeap != nullptr) _dsvHeap->Release();
    }

    void DXCSwapChain::CreateFramebuffers(std::uint32_t width, std::uint32_t height) {

        description.initialHeight = height;
        description.initialWidth = width;

        auto dxcDev = _dev;
        
        // Create a RTV for each frame.
        ID3D12DescriptorHeap* pRtvHeap = nullptr;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        heapDesc.NumDescriptors = _fbCnt;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ThrowIfFailed(dxcDev->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pRtvHeap)));
        _rtvHeap = pRtvHeap;

        auto rtvHandle = pRtvHeap->GetCPUDescriptorHandleForHeapStart();
        auto rtvDescriptorSize = dxcDev->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        
        ID3D12DescriptorHeap* pDsvHeap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle {};
        uint32_t dsvDescriptorSize = 0;
    
        if(description.depthFormat.has_value()) {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
            heapDesc.NumDescriptors = _fbCnt;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            ThrowIfFailed(dxcDev->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pDsvHeap)));
            dsvHandle = pDsvHeap->GetCPUDescriptorHandleForHeapStart();
            dsvDescriptorSize = dxcDev->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            
        }
        _dsvHeap = pDsvHeap;

        

        ITexture::Description texDesc{};  
        texDesc.type = alloy::ITexture::Description::Type::Texture2D;
        texDesc.width = width;
        texDesc.height = height;
        texDesc.depth = 1;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.usage.renderTarget = true;
        texDesc.sampleCount = SampleCount::x1;
        texDesc.format = D3DToVdPixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM);///#TODO: add format support
            
        for (uint32_t n = 0; n < _fbCnt; n++)
        {
            ID3D12Resource* pTex;
            ThrowIfFailed(_sc->GetBuffer(n, IID_PPV_ARGS(&pTex)));
            dxcDev->GetDevice()->CreateRenderTargetView(pTex, nullptr, rtvHandle);

            auto vldTex = DXCTexture::WrapNative(dxcDev, texDesc, pTex);

            //IFrameBuffer::Description fbDesc;
            //fbDesc.colorTargets = { 
            //    {vldTex, vldTex->GetDesc().arrayLayers, vldTex->GetDesc().mipLevels}
            //};

            common::sp<ITexture> vldDepthTex;
            if (description.depthFormat.has_value()) {

                ITexture::Description dsDesc{};
                dsDesc.type = alloy::ITexture::Description::Type::Texture2D;
                dsDesc.width = width;
                dsDesc.height = height;
                dsDesc.depth = 1;
                dsDesc.mipLevels = 1;
                dsDesc.arrayLayers = 1;
                dsDesc.usage.depthStencil = true;
                dsDesc.sampleCount = SampleCount::x1;
                dsDesc.format = description.depthFormat.value();

                vldDepthTex = _dev->GetResourceFactory().CreateTexture(dsDesc);

                //fbDesc.depthTarget = { vldDepthTex, vldDepthTex->GetDesc().arrayLayers, vldDepthTex->GetDesc().mipLevels };

                auto dxcDepthTex = PtrCast<DXCTexture>(vldDepthTex.get());
                dxcDev->GetDevice()->CreateDepthStencilView(dxcDepthTex->GetHandle(), nullptr, dsvHandle);
        
            }

            auto& fbC = _fbs.emplace_back();
            ITextureView::Description ctvDesc { };
            ctvDesc.arrayLayers = 1;
            ctvDesc.mipLevels = 1;
            fbC.colorTgt.tex = DXCTextureView::Make(vldTex, ctvDesc);
            fbC.colorTgt.view = rtvHandle;
            if(vldDepthTex) {
                auto dxcDepthTex = PtrCast<DXCTexture>(vldDepthTex.get());
                dxcDepthTex->ref();
                
                ITextureView::Description dsvDesc { };
                dsvDesc.arrayLayers = 1;
                dsvDesc.mipLevels = 1;
                fbC.dsTgt.tex = DXCTextureView::Make(common::sp(dxcDepthTex), dsvDesc);
                fbC.dsTgt.view = dsvHandle;
            }

            rtvHandle.ptr += rtvDescriptorSize;
            dsvHandle.ptr += dsvDescriptorSize;
        }
    }

}
