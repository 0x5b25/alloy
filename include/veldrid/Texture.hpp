#pragma once

#include "veldrid/common/Macros.h"
#include "veldrid/common/RefCnt.hpp"
#include "veldrid/Types.hpp"
#include "veldrid/DeviceResource.hpp"
#include "veldrid/BindableResource.hpp"

#include <cstdint>

namespace Veldrid
{
    

    class Texture : public DeviceResource{

    public:
        struct Description
        {
            
            std::uint32_t width, height, depth;
        
            std::uint32_t mipLevels;
            std::uint32_t arrayLayers;
            PixelFormat format;

            union Usage{
                struct {
                    std::uint8_t sampled : 1;
                    std::uint8_t storage : 1;
                    std::uint8_t renderTarget : 1;
                    std::uint8_t depthStencil : 1;
                    std::uint8_t cubemap : 1;
                    //bool staging : 1;
                    std::uint8_t generateMipmaps : 1;
                };
                std::uint8_t value;
            } usage;
            //enum class UsageFlags: std::uint8_t{
            //    /// <summary>
            //    /// The Texture can be used as the target of a read-only <see cref="TextureView"/>, and can be accessed from a shader.
            //    /// </summary>
            //    Sampled = 1 << 0,
            //    /// <summary>
            //    /// The Texture can be used as the target of a read-write <see cref="TextureView"/>, and can be accessed from a shader.
            //    /// </summary>
            //    Storage = 1 << 1,
            //    /// <summary>
            //    /// The Texture can be used as the color target of a <see cref="Framebuffer"/>.
            //    /// </summary>
            //    RenderTarget = 1 << 2,
            //    /// <summary>
            //    /// The Texture can be used as the depth target of a <see cref="Framebuffer"/>.
            //    /// </summary>
            //    DepthStencil = 1 << 3,
            //    /// <summary>
            //    /// The Texture is a two-dimensional cubemap.
            //    /// </summary>
            //    Cubemap = 1 << 4,
            //    /// <summary>
            //    /// The Texture is used as a read-write staging resource for uploading Texture data.
            //    /// With this flag, a Texture can be mapped using the <see cref="GraphicsDevice.Map(MappableResource, MapMode, uint)"/>
            //    /// method.
            //    /// </summary>
            //    Staging = 1 << 5,
            //    /// <summary>
            //    /// The Texture supports automatic generation of mipmaps through <see cref="CommandList.GenerateMipmaps(Texture)"/>.
            //    /// </summary>
            //    GenerateMipmaps = 1 << 6,
            //};

            //std::uint32_t usage;

            enum class Type: uint8_t{
                Texture1D, Texture2D, Texture3D
            } type;

            enum class SampleCount : uint8_t{
                x1 = 1,
                x2 = 2,
                x4 = 4,
                x8 = 8,
                x16 = 16,
                x32 = 32
            } sampleCount;
        };
        
    protected:
        Description description;

    protected:
        Texture(
            const sp<GraphicsDevice>& dev,
            const Texture::Description& desc
        ) : 
            DeviceResource(std::move(dev)),
            description(desc)
        {}

    public:
        const Description& GetDesc() const {return description;}

    };


    class TextureView : public IBindableResource{


    public:
        struct Description
        {
            std::uint32_t baseMipLevel;
            std::uint32_t mipLevels;
            std::uint32_t baseArrayLayer;
            std::uint32_t arrayLayers;
            PixelFormat format;
        };
    
    protected:
        Description description;
        sp<Texture> target;

    protected:
        TextureView(
            sp<Texture>&& target,
            const TextureView::Description& desc
        ) :
            description(desc),
            target(std::move(target))
        {}

    public:
        const Description& GetDesc() const {
            return description;
        }

        virtual ResourceKind GetResourceKind() const override { return ResourceKind::Sampler; }

        const sp<Texture>& GetTarget() const { return target; }

    };

} // namespace Veldrid
