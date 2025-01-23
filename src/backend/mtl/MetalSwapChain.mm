#include "MetalSwapChain.h"
#include "MetalTexture.h"
#include "alloy/SwapChain.hpp"
#include "alloy/SwapChainSources.hpp"

#include "MtlTypeCvt.h"
#include "MtlSurfaceUtil.hpp"

//#include <AppKit/AppKit.h>
#import <AppKit/NSWindow.h>


namespace alloy::mtl {

common::sp<MetalSwapChain> MetalSwapChain::Make(
        const common::sp<MetalDevice> &dev,
        const Description& desc
) {
    @autoreleasepool{
        
        //auto surf = CreateSurface(dev->GetHandle(), desc.source);
        //[[MTLRenderPassDescriptor alloc] init];
        CAMetalLayer *swapChain = [CAMetalLayer new];

        swapChain.device = dev->GetHandle();
        //swapChain.opaque = true;
        //swapChain.pixelFormat = VdToMtlPixelFormat(desc.colorFormat);
        //Metal specs say that drawable count can only be 2 or 3
        swapChain.maximumDrawableCount = desc.backBufferCnt;
        swapChain.drawableSize = {(float)desc.initialWidth, (float)desc.initialHeight};

        if(desc.source){
            switch(desc.source->tag){

            case SwapChainSource::Tag::NSView: {
                auto NSViewSrc = (NSViewSwapChainSource*)desc.source;

                auto nsView = (NSView*)NSViewSrc->nsView;

                nsView.layer = swapChain;
                nsView.wantsLayer = true;

            } break;
            case SwapChainSource::Tag::NSWindow:{
                auto NSWndSrc = (NSWindowSwapChainSource*)desc.source;

                NSWindow* nsWnd = (NSWindow*)NSWndSrc->nsWindow;

                nsWnd.contentView.layer = swapChain;
                nsWnd.contentView.wantsLayer = true;
            } break;
            case SwapChainSource::Tag::Win32:
            default:
            break;
            }
        }
        //[swapChain retain];
        auto sc = new MetalSwapChain(dev, swapChain, desc);

        sc->CreateFramebuffers(desc.initialWidth, desc.initialHeight);

        return common::sp(sc);
    }
}

std::uint32_t MetalSwapChain::GetWidth() const {
    return _layer.drawableSize.width;
}
std::uint32_t MetalSwapChain::GetHeight() const {

    return _layer.drawableSize.height;
}


    MetalSwapChain::MetalSwapChain(
        const common::sp<MetalDevice> &dev, 
        CAMetalLayer *layer,
        const Description& desc
    )
        : ISwapChain(desc)
        , _layer(layer)
        , _dev(dev)
        , _capturingFb(nullptr)
        , _currentDs(0)
    {
        //auto mtlDev = static_cast<DeviceImpl*>(dev.get());
        //mtlDev->ref();
        //_dev.reset(mtlDev);
    }

    MetalSwapChain::~MetalSwapChain(){
        @autoreleasepool {
            [_layer release];
        }
    }

    void MetalSwapChain::Resize(unsigned width, unsigned height) {
        @autoreleasepool {
            ReleaseFramebuffers();
            _layer.drawableSize = {(float)width, (float)height};
            CreateFramebuffers(width, height);
        }
    }

    void MetalSwapChain::ReleaseFramebuffers() {
        assert(_capturingFb == nullptr);
        _currentDs = 0;
        _dsTex.clear();
    }

    void MetalSwapChain::CreateFramebuffers(std::uint32_t width, std::uint32_t height) {
        if (description.depthFormat.has_value()) {
            for(uint32_t i = 0; i < description.backBufferCnt; i++)
            {

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

                auto vldDepthTex = _dev->GetResourceFactory().CreateTexture(dsDesc);
                auto mtlDepthTex = common::SPCast<MetalTexture>(vldDepthTex);

                _dsTex.push_back(mtlDepthTex);
            }
        }
    }


    common::sp<IFrameBuffer> MetalSwapChain::GetBackBuffer() {
        ///#TODO: Not thread safe.
        if(_capturingFb) {
            _capturingFb->ref();
        }
        else {
            @autoreleasepool{

                auto drawable = [_layer nextDrawable];

                common::sp<MetalTexture> dsTex;

                if(!_dsTex.empty()) {
                    dsTex = _dsTex[_currentDs];
                    _currentDs++;
                    if(_currentDs >= _dsTex.size()) {
                        _currentDs = 0;
                    }
                }

                _capturingFb = new MetalSCFB(common::ref_sp(this), drawable, dsTex);
            }
        }

        return common::sp(_capturingFb);
    }


    void MetalSwapChain::PresentBackBuffer() {
        assert(_capturingFb != nullptr);
        //assert(_capturingFb->unique());

        auto drawable = _capturingFb->GetDrawable();

        [drawable present];

        //_capturingFb->unref();
        _capturingFb = nullptr;

    }


    OutputDescription MetalSCFB::GetDesc() {
        OutputDescription desc { };

        auto rawTex = _drawable.texture;

        ITexture::Description texDesc{};
        
        switch([rawTex textureType]) {
                
            case MTLTextureType1D: texDesc.type = ITexture::Description::Type::Texture1D; break;
            case MTLTextureType2D: texDesc.type = ITexture::Description::Type::Texture1D; break;
            case MTLTextureType3D: texDesc.type = ITexture::Description::Type::Texture3D; break;
            default:
                
                break;
        }
        
        texDesc.width = [rawTex width];
        texDesc.height = [rawTex height];
        texDesc.depth = [rawTex depth];
        texDesc.mipLevels = [rawTex mipmapLevelCount];
        texDesc.arrayLayers = [rawTex arrayLength];
        texDesc.usage.renderTarget = true;
        switch([rawTex sampleCount]) {
            case 1: texDesc.sampleCount = SampleCount::x1; break;
            case 2: texDesc.sampleCount = SampleCount::x2; break;
            case 4: texDesc.sampleCount = SampleCount::x4; break;
            case 8: texDesc.sampleCount = SampleCount::x8; break;
            case 16: texDesc.sampleCount = SampleCount::x16; break;
            default:
                break;
        }
        
        texDesc.format = MtlToAlPixelFormat([rawTex pixelFormat]);///#TODO: add format support
            

        auto vldTex = MetalTexture::WrapNative(_sc->GetDevice(), texDesc, rawTex);


        ITextureView::Description colorViewDesc{};
        colorViewDesc.arrayLayers = 1;
        colorViewDesc.mipLevels = 1;
        auto colorView = new MetalTextureView(vldTex, colorViewDesc);

        desc.colorAttachments.push_back(common::sp(new MetalSCRT(
            common::ref_sp(this),
            common::sp(colorView)
        )));

        if(_dsTex) {
            ITextureView::Description dsViewDesc{};
            dsViewDesc.arrayLayers = 1;
            dsViewDesc.mipLevels = 1;
            auto dsView = new MetalTextureView(common::ref_sp(_dsTex.get()), dsViewDesc);
            desc.depthAttachment.reset(new MetalSCRT(
                common::ref_sp(this),
                common::sp(dsView)
            ));
        }

        desc.sampleCount = SampleCount::x1; //_bb.colorTgt.tex->GetTextureObject()->GetDesc().sampleCount;

        return desc;

    }
}
