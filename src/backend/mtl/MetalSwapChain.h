#pragma once

#include "MetalDevice.h"
#include "MetalTexture.h"
#include "alloy/Framebuffer.hpp"
#include "alloy/SwapChain.hpp"

#include <cassert>
#include <vector>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>


namespace alloy::mtl{

    class MetalSCFB;

class MetalSwapChain : public ISwapChain{
    common::sp<MetalDevice> _dev;
    CAMetalLayer* _layer;

    std::vector<common::sp<MetalTexture>> _dsTex;
    uint32_t _currentDs;
    id<CAMetalDrawable> _currentCt;
    
    //MetalSCFB* _capturingFb;
    
public:

    //DeviceImpl* GetDev() const {return _dev.get();}

    MetalSwapChain( const common::sp<MetalDevice> &dev, 
                    CAMetalLayer *layer,
                    const Description& desc);

    static common::sp<MetalSwapChain> Make(
        const common::sp<MetalDevice> &dev,
        const Description& desc
    );

    virtual ~MetalSwapChain();

    virtual void Resize(unsigned width, unsigned height) override;
    
    virtual common::sp<IFrameBuffer> GetBackBuffer() override;
    
    common::sp<MetalDevice> GetDevice() const { return _dev; }

    void BackBufferReleased(MetalSCFB* which) {
        //assert(_capturingFb != nullptr);
        //assert(which == _capturingFb);
        //if(which == _capturingFb)
        //    _capturingFb = nullptr;
    }

    virtual bool IsSyncToVerticalBlank() const override { return false; }
    virtual void SetSyncToVerticalBlank(bool sync) override {}

    bool HasDepthTarget() const {return description.depthFormat.has_value();}

    virtual std::uint32_t GetWidth() const override;
    virtual std::uint32_t GetHeight() const override;


    void ReleaseFramebuffers();
    void CreateFramebuffers(std::uint32_t width, std::uint32_t height);

    void PresentBackBuffer();

};

    class MetalSCFB : public IFrameBuffer {
        common::sp<MetalSwapChain> _sc;
        id<CAMetalDrawable> _drawable;
        common::sp<MetalTexture> _dsTex;

    public:

        MetalSCFB(
            const common::sp<MetalSwapChain>& sc,
            id<CAMetalDrawable> drawable,
            const common::sp<MetalTexture>& depthStencil
        )
            : _sc(sc)
            , _drawable(drawable)
            , _dsTex(depthStencil)
        { 
            [_drawable retain];
        }

        virtual ~MetalSCFB() {

            [_drawable release];
            _sc->BackBufferReleased(this);
        }

        virtual OutputDescription GetDesc() override;

        id<CAMetalDrawable> GetDrawable() const {return _drawable;}
    };

    class MetalSCRT : public IRenderTarget{

        common::sp<MetalSCFB> _fb;
        common::sp<MetalTextureView> _view;

    public:
        MetalSCRT(
            const common::sp<MetalSCFB>& fb,
            const common::sp<MetalTextureView>& view
        )
            : _fb(fb)
            , _view(view)
        {
        }

        virtual ~MetalSCRT() override {
        }


        virtual ITextureView& GetTexture() const override {return *_view.get();}


    };

}
