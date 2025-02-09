#pragma once

#include "alloy/common/RefCnt.hpp"

#include "alloy/Texture.hpp"
#include "alloy/Sampler.hpp"
#include "alloy/FrameBuffer.hpp"

#import <Metal/Metal.h>

namespace alloy::mtl {

class MetalDevice;

    class MetalTexture : public ITexture {
        common::sp<MetalDevice> _dev;

        id<MTLTexture> _tex;
    public:

        MetalTexture(
            const common::sp<MetalDevice>& dev,
            const ITexture::Description& desc,
            id<MTLTexture> tex);

        virtual ~MetalTexture() override;


        id<MTLTexture> GetHandle() const {return _tex;}


        static common::sp<MetalTexture> WrapNative(
            const common::sp<MetalDevice>& dev,
            const ITexture::Description& desc,
            id<MTLTexture> nativeRes
        );
        
        static common::sp<MetalTexture> Make(const common::sp<MetalDevice> &dev,
                                             const ITexture::Description &desc);

        virtual void WriteSubresource(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            Point3D dstOrigin,
            Size3D writeSize,
            const void* src,
            uint32_t srcRowPitch,
            uint32_t srcDepthPitch
        ) override;

        virtual void ReadSubresource(
            void* dst,
            uint32_t dstRowPitch,
            uint32_t dstDepthPitch,
            uint32_t mipLevel,
            uint32_t arrayLayer,
            Point3D srcOrigin,
            Size3D readSize
        ) override;

        virtual SubresourceLayout GetSubresourceLayout(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            SubresourceAspect aspect = SubresourceAspect::Color) override;

    };

    class MetalTextureView : public ITextureView {

    public:

        MetalTextureView(
            common::sp<ITexture>&& target,
            const ITextureView::Description& desc
        ) 
            : ITextureView(std::move(target), desc)
        {}

    public:
        virtual ~MetalTextureView() {}
        
    };

    class MetalSampler : public ISampler {
        
        common::sp<MetalDevice> _dev;
        id<MTLSamplerState> _sampler;
        
    public:
        
        
        MetalSampler(
            const common::sp<MetalDevice>& dev,
            const ISampler::Description& desc,
            id<MTLSamplerState> sampler
        );
        
        virtual ~MetalSampler() override;
        
        id<MTLSamplerState> GetHandle() const {return _sampler;}
        
        static common::sp<MetalSampler> Make(const common::sp<MetalDevice> &dev,
                                             const ISampler::Description &desc);
        
    };

    class MetalRenderTarget : public IRenderTarget {

        common::sp<MetalTextureView> _texView;
    public:
        MetalRenderTarget(const common::sp<MetalTextureView>& texView);
        virtual ~MetalRenderTarget();

        virtual ITextureView& GetTexture() const override;

        static common::sp<MetalRenderTarget> Make(
            const common::sp<MetalTextureView>& texView
        );
    
    };

}
