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

    class MetalGfxPipelineBase {
    protected:
        common::sp<MetalDevice> mtlDev;
        id<MTLRenderPipelineState> mtlPipelineState;
        common::sp<MetalResourceLayout> pipelineLayout;
    public:

        MetalGfxPipelineBase(const common::sp<MetalDevice>& dev,
                          id<MTLRenderPipelineState> pipeline,
                          const common::sp<MetalResourceLayout>& layout);

        virtual ~MetalGfxPipelineBase();

        common::sp<MetalResourceLayout> GetPipelineLayout() const;

        virtual void BindToCmdBuf(id<MTLCommandEncoder> encoder) = 0;
#ifdef VLD_DEBUG
        virtual const void* GetPipelineType() const = 0;
#endif
    };


    class MetalGfxPipeline : public MetalGfxPipelineBase
                           , public alloy::IGfxPipeline {


        MTLPrimitiveType _mtlPrimitiveTopology;

        _MtlRasteizerState _rasterizerState;
        Color4f _blendColor;

        id<MTLDepthStencilState> _mtlDepthStencilState;

        //Hold reference to resources
        std::vector<common::sp<IShader> > _shaders;

        std::vector<VertexLayout> _vertexLayouts;

        MetalGfxPipeline(const common::sp<MetalDevice>& dev);

    public:

        virtual ~MetalGfxPipeline() override;


        static common::sp<MetalGfxPipeline> Make(
            common::sp<MetalDevice>&&,
            const alloy::GraphicsPipelineDescription& desc);

        //virtual PipelineKind GetPipelineKind() const override {return PipelineKind::Gfx;}
        void BindToCmdBuf(id<MTLCommandEncoder> enc) override;

        const std::vector<VertexLayout>& GetVertexLayouts() const {
            return _vertexLayouts;
        }

        MTLPrimitiveType GetPrimitiveTopology() const {return _mtlPrimitiveTopology;}

#ifdef VLD_DEBUG
        static const void* GetTypeKey() {return (const void*)&MetalGfxPipeline::Make;}
        virtual const void* GetPipelineType() const override {
            return (const void*)&MetalGfxPipeline::Make;
        }
#endif
    };

    class MetalComputePipeline : public alloy::IComputePipeline {

        common::sp<MetalDevice> _mtlDev;
        id<MTLComputePipelineState> _mtlPipelineState;
        common::sp<MetalResourceLayout> _pipelineLayout;

        struct {
            uint32_t wgSize [3];
        } _meta;

        MetalComputePipeline(const common::sp<MetalDevice>& dev);
    public:

        virtual ~MetalComputePipeline() override;

        static common::sp<MetalComputePipeline> Make(
            common::sp<MetalDevice>&&,
            const alloy::ComputePipelineDescription& desc);

        common::sp<MetalResourceLayout> GetPipelineLayout() const;

        Size3D GetWorkgroupSize() const {
            return {_meta.wgSize[0], _meta.wgSize[1], _meta.wgSize[1]};
        }

        void BindToCmdBuf(id<MTLComputeCommandEncoder> encoder);

    };

    class MetalMeshShaderPipeline : public MetalGfxPipelineBase
                                  , public alloy::IMeshShaderPipeline {

        _MtlRasteizerState _rasterizerState;
        Color4f _blendColor;

        struct {
            uint32_t taskShaderWGSize[3];
            uint32_t meshShaderWGSize[3];
            uint32_t maxPayloadSize;
            uint32_t maxVertOutputCnt;
            uint32_t maxPrimitiveOutputCnt;
            //Should only use triangle, line & point
            MTLPrimitiveTopologyClass primType;
        } _meta;

        id<MTLDepthStencilState> _mtlDepthStencilState;

        //Hold reference to resources
        std::vector<common::sp<IShader> > _shaders;

        MetalMeshShaderPipeline(const common::sp<MetalDevice>& dev);

    public:

        virtual ~MetalMeshShaderPipeline() override;

        static common::sp<MetalMeshShaderPipeline> Make(
            common::sp<MetalDevice>&&,
            const alloy::MeshShaderPipelineDescription& desc);


        Size3D GetTaskShaderWorkgroupSize() const {
            auto& wgSize = _meta.taskShaderWGSize;
            return {wgSize[0], wgSize[1], wgSize[1]};
        }

        Size3D GetMeshShaderWorkgroupSize() const {
            auto& wgSize = _meta.meshShaderWGSize;
            return {wgSize[0], wgSize[1], wgSize[1]};
        }

        virtual void BindToCmdBuf(id<MTLCommandEncoder> enc) override;

        //const std::vector<VertexLayout>& GetVertexLayouts() const {
        //    return _vertexLayouts;
        //}
        //MTLPrimitiveType GetPrimitiveTopology() const {return _mtlPrimitiveTopology;}

#ifdef VLD_DEBUG
        static const void* GetTypeKey() {return (const void*)&MetalMeshShaderPipeline::Make;}

        virtual const void* GetPipelineType() const override {
            return (const void*)&MetalMeshShaderPipeline::Make;
        }
#endif

    };

#ifdef VLD_DEBUG
    inline bool IsGfxPipeline(const MetalGfxPipelineBase& pipe) {
        return pipe.GetPipelineType() == MetalGfxPipeline::GetTypeKey();
    }

    inline bool IsMeshShaderPipeline(const MetalGfxPipelineBase& pipe) {
        return pipe.GetPipelineType() == MetalMeshShaderPipeline::GetTypeKey();
    }

#endif
}
