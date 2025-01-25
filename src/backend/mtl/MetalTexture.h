#pragma once

#include "alloy/common/RefCnt.hpp"

#include "alloy/Texture.hpp"
#include "alloy/Sampler.hpp"

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
         )
            : ISampler(desc)
            , _dev(dev)
            , _sampler(sampler)
        { }
        
        virtual ~MetalSampler() override {
            @autoreleasepool {
                [_sampler release];
            }
        }
        
        id<MTLSamplerState> GetHandle() const {return _sampler;}
        
        static common::sp<MetalSampler> Make(const common::sp<MetalDevice> &dev,
                                             const ISampler::Description &desc);
        
    };

}
