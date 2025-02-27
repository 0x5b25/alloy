#pragma once


//3rd-party headers
#include <d3d12.h>
#include <D3D12MemAlloc.h>

#include "alloy/Texture.hpp"
#include "alloy/Sampler.hpp"

#include <vector>

namespace alloy::dxc
{
    class DXCDevice;
    
    class DXCTexture : public ITexture{
    public:
        static uint32_t ComputeSubresource(uint32_t mipLevel, uint32_t mipLevelCount, uint32_t arrayLayer);
    private:
        common::sp<DXCDevice> dev;
        D3D12MA::Allocation* _allocation;
        ID3D12Resource* _res;

        //std::vector<VkImageLayout> _imageLayouts;

        //Layout tracking
        //VkImageLayout _layout;
        //VkAccessFlags _accessFlag;
        //VkPipelineStageFlags _pipelineFlag;


        DXCTexture(
            const common::sp<DXCDevice>& dev,
            const ITexture::Description& desc
        ) 
            : ITexture( desc) 
            , dev(dev)
        { }

        void SetResource(ID3D12Resource* res);


    public:

        virtual ~DXCTexture() override;

        ID3D12Resource* GetHandle() const { return _res; }
        
        virtual void* GetNativeHandle() const override {return GetHandle();}

        DXCDevice* GetDevice() const { return dev.get(); }
        bool IsOwnTexture() const {return _allocation != nullptr; }

        static common::sp<ITexture> Make(
            const common::sp<DXCDevice>& dev,
            const ITexture::Description& desc
        );

        static common::sp<DXCTexture> WrapNative(
            const common::sp<DXCDevice>& dev,
            const ITexture::Description& desc,
            ID3D12Resource* nativeRes
        );

        
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

    

    class DXCTextureView : public ITextureView {
    
    
    public:
        DXCTextureView(
            common::sp<ITexture>&& target,
            const ITextureView::Description& desc
        ) :
            ITextureView(std::move(target),desc)
        {}
    
    
        ~DXCTextureView() {}

        static common::sp<DXCTextureView> Make (
            const common::sp<DXCTexture>& tex,
            const ITextureView::Description& desc
        );
        
    };

    class DXCSampler : public ISampler{
    
        common::sp<DXCDevice> _dev;
    
        DXCSampler(
            const common::sp<DXCDevice>& dev,
            const ISampler::Description& desc
        ) 
          : ISampler(desc)
          ,  _dev(dev)
        {}
    
    public:
    
        ~DXCSampler() {}
    
    
        static common::sp<ISampler> Make(
            const common::sp<DXCDevice>& dev,
            const ISampler::Description& desc
        ) {
            return common::sp(new DXCSampler(dev, desc));
        }
    
    };

} // namespace alloy
