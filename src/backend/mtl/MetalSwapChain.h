#pragma once

#include "MetalDevice.h"
#include "MetalTexture.h"
#include "alloy/SwapChain.hpp"

#include <cassert>
#include <vector>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>


namespace alloy::mtl{

    class MetalSwapChain;

    class MetalSCTexView : public MetalTextureView{

        common::sp<MetalSwapChain> _sc;

    public:
        MetalSCTexView (
            common::sp<MetalTexture> target,
            const ITextureView::Description& desc,
            common::sp<MetalSwapChain> sc
        )
            : MetalTextureView(std::move(target), desc)
            , _sc(std::move(sc))
        {

        }
    };

    class MetalSwapChain : public ISwapChain{
        common::sp<MetalDevice> _dev;
        CAMetalLayer* _layer;

        // Metal doesn't give us this value. Let's assume metal's
        // nextDrawable will return images in order.
        uint32_t _currentFrameIdx;
        id<CAMetalDrawable> _currentCt;

        //MetalSCFB* _capturingFb;

        Description _desc;

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

        virtual const Description& GetDesc() const override {
            return _desc;
        }

        virtual void Resize(unsigned width, unsigned height) override;

        virtual common::sp<ITextureView> GetBackBuffer() override;

        virtual uint32_t GetBackBufferIndex() override { return _currentFrameIdx; }

        common::sp<MetalDevice> GetDevice() const { return _dev; }

        virtual bool IsSyncToVerticalBlank() const override { return false; }
        virtual void SetSyncToVerticalBlank(bool sync) override {}

        virtual std::uint32_t GetWidth() const override;
        virtual std::uint32_t GetHeight() const override;


        void ReleaseFramebuffers();
        void CreateFramebuffers(std::uint32_t width, std::uint32_t height);

        void PresentBackBuffer();

    };

}
