#include "MetalTexture.h"

#include "MetalDevice.h"
#include "MtlTypeCvt.h"

namespace alloy::mtl  {


    common::sp<MetalTexture> MetalTexture::WrapNative(
        const common::sp<MetalDevice>& dev,
        const ITexture::Description& desc,
        id<MTLTexture> nativeRes
    ) {
        ///#TODO: do we need to differentiate wrap native and normal created?
        ///
        [nativeRes retain];
        return common::sp(new MetalTexture(dev, desc, nativeRes));
    }

    common::sp<MetalTexture> MetalTexture::Make(
        const common::sp<MetalDevice>& dev,
        const ITexture::Description& desc
    ) {
        @autoreleasepool {
            
            
            MTLTextureDescriptor* mtlDesc = [[MTLTextureDescriptor new] autorelease];
            
            
            // Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
            // an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
            mtlDesc.pixelFormat = AlToMtlPixelFormat(desc.format);
            
            switch (desc.type) {
                case ITexture::Description::Type::Texture1D : mtlDesc.textureType = MTLTextureType1DArray; break;
                case ITexture::Description::Type::Texture2D : mtlDesc.textureType = MTLTextureType2DArray; break;
                case ITexture::Description::Type::Texture3D : mtlDesc.textureType = MTLTextureType3D; break;
            }
            
            mtlDesc.width = desc.width;
            mtlDesc.height = desc.height;
            mtlDesc.depth = desc.depth;
            mtlDesc.mipmapLevelCount = desc.mipLevels;
            mtlDesc.arrayLength = desc.arrayLayers;
            switch(desc.sampleCount){
                case SampleCount::x1: mtlDesc.sampleCount = 1; break;
                case SampleCount::x2: mtlDesc.sampleCount = 2; break;
                case SampleCount::x4: mtlDesc.sampleCount = 4; break;
                case SampleCount::x8: mtlDesc.sampleCount = 8; break;
                case SampleCount::x16:mtlDesc.sampleCount = 16; break;
                case SampleCount::x32:mtlDesc.sampleCount = 32; break;
            }
            
            switch(desc.hostAccess) {
                case HostAccess::PreferSystemMemory:
                case HostAccess::PreferDeviceMemory:
                    mtlDesc.storageMode = MTLStorageModeShared;
                    break;
                case HostAccess::None:
                default:
                    mtlDesc.storageMode = MTLStorageModePrivate;
                    break;
            }
            
            if(desc.usage.renderTarget || desc.usage.depthStencil)
                mtlDesc.usage |= MTLTextureUsageRenderTarget;
            if(desc.usage.sampled)
                mtlDesc.usage |= MTLTextureUsageShaderRead;
            if(desc.usage.storage)
                mtlDesc.usage |= MTLTextureUsageShaderRead
                               | MTLTextureUsageShaderRead;
            
            
            // Create the texture from the device by using the descriptor
            id<MTLTexture> texture = [dev->GetHandle() newTextureWithDescriptor:mtlDesc];
            
            
            return common::sp(new MetalTexture(dev, desc, texture));
            
        }

    }


    MetalTexture::MetalTexture(
        const common::sp<MetalDevice>& dev,
        const ITexture::Description& desc,
        id<MTLTexture> tex
    )
        : ITexture(desc)
        , _dev(dev)
        , _tex(tex)
    {
    }

    MetalTexture::~MetalTexture() {

        [_tex release];
    }

    void MetalTexture::WriteSubresource(uint32_t mipLevel,
                                        uint32_t arrayLayer,
                                        Point3D dstOrigin,
                                        Size3D writeSize,
                                        const void* src,
                                        uint32_t srcRowPitch,
                                        uint32_t srcDepthPitch)
    {
        @autoreleasepool {
            MTLRegion region {
                .origin = MTLOriginMake(dstOrigin.x, dstOrigin.y, dstOrigin.z),
                .size = MTLSizeMake(writeSize.width, writeSize.height, writeSize.depth)
            };
            
            
            [_tex replaceRegion:region
                    mipmapLevel:mipLevel
                          slice:arrayLayer
                      withBytes:src
                    bytesPerRow:srcRowPitch
                  bytesPerImage:srcDepthPitch];
        }
    }

    void MetalTexture::ReadSubresource(void* dst,
                                       uint32_t dstRowPitch,
                                       uint32_t dstDepthPitch,
                                       uint32_t mipLevel,
                                       uint32_t arrayLayer,
                                       Point3D srcOrigin,
                                       Size3D readSize)
    {
        @autoreleasepool {
            MTLRegion region {
                .origin = MTLOriginMake(srcOrigin.x, srcOrigin.y, srcOrigin.z),
                .size = MTLSizeMake(readSize.width, readSize.height, readSize.depth)
            };
            
            
            [_tex getBytes:dst
               bytesPerRow:dstRowPitch
             bytesPerImage:dstDepthPitch
                fromRegion:region
               mipmapLevel:mipLevel
                     slice:arrayLayer];
        }
    }

    ITexture::SubresourceLayout MetalTexture::GetSubresourceLayout(uint32_t mipLevel,
                                                                   uint32_t arrayLayer,
                                                                   SubresourceAspect aspect)
    {
        return {};
    }

    

    common::sp<MetalSampler> MetalSampler::Make(const common::sp<MetalDevice> &dev,
                                     const ISampler::Description &desc)
    {
        auto _AlToMtlAddressMode = [](Description::AddressMode mode)->MTLSamplerAddressMode{
            switch(mode) {
                case ISampler::Description::AddressMode::Mirror : return MTLSamplerAddressModeMirrorRepeat;
                case ISampler::Description::AddressMode::Clamp : return MTLSamplerAddressModeClampToEdge;
                case ISampler::Description::AddressMode::Border : return MTLSamplerAddressModeClampToBorderColor;
                case ISampler::Description::AddressMode::Wrap :
                default:
                    return MTLSamplerAddressModeRepeat;
            }
        };
        
        @autoreleasepool {
            auto mtlDesc = [[MTLSamplerDescriptor new] autorelease];

            mtlDesc.sAddressMode = _AlToMtlAddressMode(desc.addressModeU);
            mtlDesc.tAddressMode = _AlToMtlAddressMode(desc.addressModeV);
            mtlDesc.rAddressMode = _AlToMtlAddressMode(desc.addressModeW);
            
            mtlDesc.maxAnisotropy = desc.maximumAnisotropy;
            mtlDesc.lodMaxClamp = desc.maximumLod;
            mtlDesc.lodMinClamp = desc.minimumLod;
            
            switch(desc.borderColor) {
                    
                case Description::BorderColor::OpaqueBlack: mtlDesc.borderColor = MTLSamplerBorderColorOpaqueBlack; break;
                case Description::BorderColor::OpaqueWhite: mtlDesc.borderColor = MTLSamplerBorderColorOpaqueWhite; break;
                case Description::BorderColor::TransparentBlack:
                default:
                    mtlDesc.borderColor = MTLSamplerBorderColorTransparentBlack; break;
            }
            
            {
                using SamplerFilter = typename ISampler::Description::SamplerFilter;
                switch (desc.filter)
                {
                    case SamplerFilter::Anisotropic:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.mipFilter = MTLSamplerMipFilterLinear;
                        break;
                    case SamplerFilter::MinPoint_MagPoint_MipPoint:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.mipFilter = MTLSamplerMipFilterNearest;
                        break;
                    case SamplerFilter::MinPoint_MagPoint_MipLinear:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.mipFilter = MTLSamplerMipFilterLinear;
                        break;
                    case SamplerFilter::MinPoint_MagLinear_MipPoint:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.mipFilter = MTLSamplerMipFilterNearest;
                        break;
                    case SamplerFilter::MinPoint_MagLinear_MipLinear:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.mipFilter = MTLSamplerMipFilterLinear;
                        break;
                    case SamplerFilter::MinLinear_MagPoint_MipPoint:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.mipFilter = MTLSamplerMipFilterNearest;
                        break;
                    case SamplerFilter::MinLinear_MagPoint_MipLinear:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterNearest;
                        mtlDesc.mipFilter = MTLSamplerMipFilterLinear;
                        break;
                    case SamplerFilter::MinLinear_MagLinear_MipPoint:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.mipFilter = MTLSamplerMipFilterNearest;
                        break;
                    case SamplerFilter::MinLinear_MagLinear_MipLinear:
                        mtlDesc.minFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.magFilter = MTLSamplerMinMagFilterLinear;
                        mtlDesc.mipFilter = MTLSamplerMipFilterLinear;
                        break;
                    default:
                        break;
                }
            }
            
            if(desc.comparisonKind) {
                switch( *desc.comparisonKind ) {
                    case ComparisonKind::Never:         mtlDesc.compareFunction = MTLCompareFunctionNever; break;
                    case ComparisonKind::Always:        mtlDesc.compareFunction = MTLCompareFunctionAlways; break;
                    case ComparisonKind::Less:          mtlDesc.compareFunction = MTLCompareFunctionLess; break;
                    case ComparisonKind::LessEqual:     mtlDesc.compareFunction = MTLCompareFunctionLessEqual; break;
                    case ComparisonKind::Equal:         mtlDesc.compareFunction = MTLCompareFunctionEqual; break;
                    case ComparisonKind::GreaterEqual:  mtlDesc.compareFunction = MTLCompareFunctionGreaterEqual; break;
                    case ComparisonKind::Greater:       mtlDesc.compareFunction = MTLCompareFunctionGreater; break;
                    case ComparisonKind::NotEqual:      mtlDesc.compareFunction = MTLCompareFunctionNotEqual; break;
                }
            }
            
            mtlDesc.supportArgumentBuffers = true;
            
            auto mtlSampler = [dev->GetHandle() newSamplerStateWithDescriptor:mtlDesc];
            
            return common::sp(new MetalSampler(dev, desc, mtlSampler));
        }
    }

}
