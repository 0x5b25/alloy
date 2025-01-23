#pragma once

#include "alloy/BindableResource.hpp"

#import <Metal/Metal.h>

#include <metal_irconverter/metal_irconverter.h>

#include <vector>

namespace alloy::mtl {

    class MetalDevice;
    class MetalResourceSet;
    
    //Implement DX12-like root signature and descriptor tables
    class MetalResourceLayout : public IResourceLayout {
        
        friend class MetalResourceSet;
    
        common::sp<MetalDevice> _dev;
        
        IRRootSignature* _rootSig;
        
    public:
        
        MetalResourceLayout(const common::sp<MetalDevice>& dev,
                            const IResourceLayout::Description& desc);
        
        virtual ~MetalResourceLayout() override;
        
        
        static common::sp<MetalResourceLayout> Make (const common::sp<MetalDevice>& dev,
                                                     const IResourceLayout::Description& desc)
        { return common::sp(new MetalResourceLayout(dev, desc)); }
        
        
        const IRRootSignature* GetHandle() const {return _rootSig;}
        
    };

    //Descriptor heap is implemented as a argument buffer
    //using explicit layout binding model for IR converter
    class MetalResourceSet : public IResourceSet {
        
        id<MTLBuffer> _argBuf;
        
    public:
        
        MetalResourceSet(const IResourceSet::Description& desc);
        
        virtual ~MetalResourceSet() override;
        
        
        
        static common::sp<MetalResourceSet> Make (const common::sp<MetalDevice>& dev,
                                                 const IResourceSet::Description& desc)
        { return common::sp(new MetalResourceSet(desc)); }
        
        const id<MTLBuffer> GetHandle() const { return _argBuf; }
        
        std::vector<id<MTLResource>> GetUseResources() const;
        
    };

}
