#pragma once


//3rd-party headers
#include <directx/d3d12.h>
#include <D3D12MemAlloc.h>

#include "veldrid/Texture.hpp"
#include "veldrid/Sampler.hpp"

#include <vector>

namespace Veldrid
{
    class DXCDevice;
    
    class DXCTexture : public Texture{

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

    

    //class DXCTextureView : public TextureView {
    //
    //    VkImageView _view;
    //
    //    DXCTextureView(
    //        const sp<DXCTexture>& target,
    //        const TextureView::Description& desc
    //    ) :
    //        TextureView(target,desc)
    //    {}
    //
    //public:
    //
    //    ~DXCTextureView();
    //
    //    const VkImageView& GetHandle() const { return _view; }
    //
    //    static sp<TextureView> Make(
    //        const sp<DXCTexture>& target,
    //        const TextureView::Description& desc
    //    );
    //
    //};

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
