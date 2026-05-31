#pragma once


//3rd-party headers
#include <d3d12.h>
#include <D3D12MemAlloc.h>

#include "alloy/Texture.hpp"
#include "alloy/Sampler.hpp"

#include <unordered_set>
namespace alloy::dxc
{
    class DXCDevice;
    class DXCTexture : public ITexture{
    public:
        static uint32_t ComputeSubresource(uint32_t mipLevel, 
                                           uint32_t mipLevelCount, 
                                           uint32_t arrayLayer, 
                                           uint32_t arrayLayerCount, 
                                           uint32_t plane);
    private:
        common::sp<DXCDevice> dev;
        D3D12MA::Allocation* _allocation;
        ID3D12Resource* _res;

        Description _desc;

        DXCTexture(
            const common::sp<DXCDevice>& dev,
            const ITexture::Description& desc
        ) 
            : _desc( desc) 
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


        virtual const Description& GetDesc() const override {return _desc;}
        
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

        virtual void SetDebugName(const std::string& name) override;
    };

    class DXCTextureView : public ITextureView {
    
        Description _desc;
        common::sp<DXCTexture> _target;
    public:
        DXCTextureView(
            common::sp<DXCTexture> target,
            const ITextureView::Description& desc
        );
    
    
        ~DXCTextureView() {}

        virtual const Description& GetDesc() const override { return _desc; }
        virtual common::sp<ITexture> GetTextureObject() const override { return _target; }      
        ID3D12Resource* GetTextureHandle() const { return _target->GetHandle(); }

        uint32_t ComputeSubresource(uint32_t mipLvl, uint32_t arrayLayer) const;
    };

    class DXCSampler : public ISampler{
    
        common::sp<DXCDevice> _dev;
        std::string _debugName;
    
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

        
        virtual void SetDebugName(const std::string& name) override {
            // We don't really have a sampler object in dx12
            _debugName = name;
        }
    
    };

} // namespace alloy
