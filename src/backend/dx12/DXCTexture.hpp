#pragma once


//3rd-party headers
#include <d3d12.h>
#include <D3D12MemAlloc.h>

#include "veldrid/Texture.hpp"
#include "veldrid/Sampler.hpp"

#include <vector>

namespace Veldrid
{
    class DXCDevice;
    
    class DXCTexture : public Texture{
    public:
        static uint32_t ComputeSubresource(uint32_t mipLevel, uint32_t mipLevelCount, uint32_t arrayLayer);
    private:

        D3D12MA::Allocation* _allocation;
        ID3D12Resource* _res;

        //std::vector<VkImageLayout> _imageLayouts;

        //Layout tracking
        //VkImageLayout _layout;
        //VkAccessFlags _accessFlag;
        //VkPipelineStageFlags _pipelineFlag;


        DXCTexture(
            const sp<GraphicsDevice>& dev,
            const Texture::Description& desc
        ) : Texture(dev, desc) {}

        void SetResource(ID3D12Resource* res);


    public:

        virtual ~DXCTexture() override;

        ID3D12Resource* GetHandle() const { return _res; }
        bool IsOwnTexture() const {return _allocation != nullptr; }
        DXCDevice* GetDevice() const;

        static sp<Texture> Make(
            const sp<DXCDevice>& dev,
            const Texture::Description& desc
        );

        static sp<Texture> WrapNative(
            const sp<DXCDevice>& dev,
            const Texture::Description& desc,
            ID3D12Resource* nativeRes
        );

        
        virtual void WriteSubresource(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            uint32_t dstX, uint32_t dstY, uint32_t dstZ,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth,
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
            uint32_t srcX, uint32_t srcY, uint32_t srcZ,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth
        ) override;

        virtual SubresourceLayout GetSubresourceLayout(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            SubresourceAspect aspect) override;


    public:
        //void TransitionImageLayout(
        //    VkCommandBuffer cb,
        //    //std::uint32_t baseMipLevel,
        //    //std::uint32_t levelCount,
        //    //std::uint32_t baseArrayLayer,
        //    //std::uint32_t layerCount,
        //    VkImageLayout layout,
        //    VkAccessFlags accessFlag,
        //    VkPipelineStageFlags pipelineFlag
        //);
        //
        //const VkImageLayout& GetLayout() const { return _layout; }
        //void SetLayout(VkImageLayout newLayout) { _layout = newLayout; }

    };


    //VkImageUsageFlags VdToVkTextureUsage(const Texture::Description::Usage& vdUsage);
    
    //VkImageType VdToVkTextureType(const Texture::Description::Type& type);

    

    class DXCTextureView : public TextureView {
    
    
    public:
        DXCTextureView(
            sp<Texture>&& target,
            const TextureView::Description& desc
        ) :
            TextureView(std::move(target),desc)
        {}
    
    
        ~DXCTextureView() {}
        
    };

    //class DXCSampler : public Sampler{
    //
    //    VkSampler _sampler;
    //
    //    sp<DXCDevice> _dev;
    //
    //    DXCSampler(
    //        const sp<DXCDevice>& dev
    //    ) :
    //        _dev(dev)
    //    {}
    //
    //public:
    //
    //    ~DXCSampler();
    //
    //    const VkSampler& GetHandle() const { return _sampler; }
    //
    //    static sp<DXCSampler> Make(
    //        const sp<DXCDevice>& dev,
    //        const Sampler::Description& desc
    //    );
    //
    //};

} // namespace Veldrid
