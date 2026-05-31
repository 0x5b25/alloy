#include "MetalSwapChain.h"
#include "MetalTexture.h"
#include "alloy/SwapChain.hpp"
#include "alloy/SwapChainSources.hpp"

#include "MtlTypeCvt.h"
#include "MtlSurfaceUtil.h"


//#include <AppKit/AppKit.h>
//#import <AppKit/NSWindow.h>


namespace alloy::mtl {

    static
    common::sp<ITextureView> _WrapDrawable(
        const MetalSwapChain* sc,
        id<CAMetalDrawable> drawable
    ) {
        auto rawTex = drawable.texture;

        ITexture::Description texDesc{};

        switch([rawTex textureType]) {

            case MTLTextureType1D: texDesc.type = ITexture::Description::Type::Texture1D; break;
            case MTLTextureType2D: texDesc.type = ITexture::Description::Type::Texture2D; break;
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


        auto vldTex = MetalTexture::WrapNative(sc->GetDevice(), texDesc, rawTex);


        ITextureView::Description colorViewDesc{};
        colorViewDesc.arrayLayers = 1;
        colorViewDesc.mipLevels = 1;
        return common::make_sp<MetalSCTexView>(
            vldTex,
            colorViewDesc,
            common::ref_sp(sc));
    }

    common::sp<MetalSwapChain> MetalSwapChain::Make(
        const common::sp<MetalDevice> &dev,
        const Description& desc
    ) {
        @autoreleasepool{

            auto swapChain = CreateSurface(dev->GetHandle(), desc.source);
            if(!swapChain) {
                return nullptr;
            }

            swapChain.device = dev->GetHandle();
            //swapChain.opaque = true;
            //swapChain.pixelFormat = VdToMtlPixelFormat(desc.colorFormat);
            //swapChain.pixelFormat = MTLPixelFormatRGBA8Unorm;
            //Metal specs say that drawable count can only be 2 or 3
            swapChain.maximumDrawableCount = desc.backBufferCnt;
            swapChain.drawableSize = {(float)desc.initialWidth, (float)desc.initialHeight};

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
        : _layer(layer)
        , _dev(dev)
        , _currentCt(nil)
        , _currentFrameIdx(0)
        , _desc(desc)
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
        if(_currentCt != nil) {
            [_currentCt release];
            _currentCt = nil;
        }
        _currentFrameIdx = 0;
    }

    void MetalSwapChain::CreateFramebuffers(std::uint32_t width, std::uint32_t height) {

    }


    common::sp<ITextureView> MetalSwapChain::GetBackBuffer() {
        ///#TODO: Not thread safe.
        //Switch to next drawable
        if(_currentCt == nil) {
            @autoreleasepool{

                _currentCt = [_layer nextDrawable];
                [_currentCt retain];

                _currentFrameIdx ++;
                if(_currentFrameIdx >= _layer.maximumDrawableCount) {
                    _currentFrameIdx = 0;
                }
            }
        }

        return _WrapDrawable(this, _currentCt);
    }


    void MetalSwapChain::PresentBackBuffer() {
        assert(_currentCt != nil);
        //assert(_capturingFb->unique());

        [_currentCt present];
        [_currentCt release];

        //_capturingFb->unref();
        _currentCt = nil;
    }

}
