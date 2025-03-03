#pragma once

#include "alloy/Pipeline.hpp"

#import <Metal/Metal.h>

namespace alloy::mtl {

    class MetalDevice;
    class MetalResourceLayout;

    
    struct _MtlRasteizerState {
        MTLWinding frontFace;
        MTLCullMode cullMode;
        MTLTriangleFillMode fillMode;
    };

    
    class MetalGfxPipeline : public alloy::IGfxPipeline {


        common::sp<MetalDevice> _mtlDev;

        id<MTLRenderPipelineState> _mtlPipelineState;

        MTLPrimitiveType _mtlPrimitiveTopology;

        _MtlRasteizerState _rasterizerState;
        Color4f _blendColor;

        id<MTLDepthStencilState> _mtlDepthStencilState;

        //Hold reference to resources
        common::sp<MetalResourceLayout> _pipelineLayout;
        std::vector<common::sp<IShader> > _shaders;
        
        std::vector<VertexLayout> _vertexLayouts;

        MetalGfxPipeline(common::sp<MetalDevice>&& dev) 
            : _mtlPipelineState(nullptr)
            , _mtlDepthStencilState(nullptr)
            , _mtlDev(std::move(dev)) {}

    public:

        virtual ~MetalGfxPipeline() override;


        static common::sp<MetalGfxPipeline> Make(
            common::sp<MetalDevice>&&, 
            const alloy::GraphicsPipelineDescription& desc);

        //virtual PipelineKind GetPipelineKind() const override {return PipelineKind::Gfx;}
        void BindToCmdBuf(id<MTLRenderCommandEncoder> enc);
        
        const std::vector<VertexLayout>& GetVertexLayouts() const {
            return _vertexLayouts;
        }

        MTLPrimitiveType GetPrimitiveTopology() const {return _mtlPrimitiveTopology;}
    
        common::sp<MetalResourceLayout> GetPipelineLayout() const;
    };

    class MetalComputePipeline : public alloy::IComputePipeline {

        common::sp<MetalDevice> _mtlDev;


        common::sp<MetalResourceLayout> _pipelineLayout;

        MetalComputePipeline(common::sp<MetalDevice>&& dev) : _mtlDev(std::move(dev)) {}
    public:


        virtual ~MetalComputePipeline() override;

        static common::sp<MetalComputePipeline> Make(
            common::sp<MetalDevice>&&, 
            const alloy::ComputePipelineDescription& desc);

    };

}
