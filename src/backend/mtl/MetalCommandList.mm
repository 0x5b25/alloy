#include "MetalCommandList.h"
#include "MetalDevice.h"
#include "MetalTexture.h"
#include "MetalBindableResource.h"
#include "MetalDescriptorHeap.hpp"
#include "MtlTypeCvt.h"
#include "alloy/RenderPass.hpp"
#include "MetalPipeline.h"

#include <Metal/MTLTypes.h>
#include <Metal/Metal.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include <cassert>
#include <cstring>

namespace alloy::mtl {

namespace {
    void SetRootArgumentBufferHeaps(
        std::vector<uint8_t>& argBuffer,
        uint64_t shaderResourceHeapGPUVA,
        uint64_t samplerHeapGPUVA
    ) {
        auto pHeap =(uint64_t*)argBuffer.data();
        if(shaderResourceHeapGPUVA) {
            *pHeap = shaderResourceHeapGPUVA;
            pHeap++;
        }
        if(samplerHeapGPUVA) {
            *pHeap = samplerHeapGPUVA;
            pHeap++;
        }
    }

    bool HasWriteAccess(const ResourceAccesses& access) {
        return access[ResourceAccess::UnorderedAccess]
            || access[ResourceAccess::RenderTarget]
            || access[ResourceAccess::DepthStencilWrite]
            || access[ResourceAccess::CopyDest]
            || access[ResourceAccess::AccelerationStructureWrite];
    }

    bool HasReadAccess(const ResourceAccesses& access) {
        return access[ResourceAccess::IndirectArgumentRead]
            || access[ResourceAccess::VertexBufferRead]
            || access[ResourceAccess::IndexBufferRead]
            || access[ResourceAccess::ConstantBufferRead]
            || access[ResourceAccess::ShaderResourceRead]
            || access[ResourceAccess::DepthStencilRead]
            || access[ResourceAccess::CopySource]
            || access[ResourceAccess::AccelerationStructureRead]
            || access[ResourceAccess::Present];
    }

    MTLResourceUsage ToMTLResourceUsage(const ResourceAccesses& access) {
        MTLResourceUsage usage = 0;

        if(HasReadAccess(access))
            usage |= MTLResourceUsageRead;

        if(HasWriteAccess(access))
            usage |= MTLResourceUsageWrite;

        return usage;
    }

    MTLRenderStages AllRenderStages() {
        return MTLRenderStageVertex
             | MTLRenderStageFragment
             | MTLRenderStageObject
             | MTLRenderStageMesh
             | MTLRenderStageTile;
    }

    MTLRenderStages ToMTLRenderStages(const PipelineStages& stages) {
        if(stages[PipelineStage::AllCommands] ||
           stages[PipelineStage::AllGraphics] ||
           stages[PipelineStage::AllShaders]
        ) {
            return AllRenderStages();
        }

        MTLRenderStages mtlStages = 0;
        if(stages[PipelineStage::VertexShader] ||
           stages[PipelineStage::VertexInput]
        ) {
            mtlStages |= MTLRenderStageVertex;
        }
        if(stages[PipelineStage::MeshShader]) {
            mtlStages |= MTLRenderStageObject | MTLRenderStageMesh;
        }
        if(stages[PipelineStage::FragmentShader] ||
           stages[PipelineStage::DepthStencil]   ||
           stages[PipelineStage::ColorOutput]
        ) {
            mtlStages |= MTLRenderStageFragment;
        }

        return mtlStages;
    }

    id<MTLResource> GetMetalResource(const common::sp<IBindableResource>& resource) {
        if(!resource) {
            return nil;
        }

        switch(resource->GetResourceKind()) {
        case IBindableResource::ResourceKind::UniformBuffer:
        case IBindableResource::ResourceKind::StorageBuffer: {
            auto* range = common::PtrCast<BufferRange>(resource.get());
            auto* mtlBuffer =
                static_cast<const MetalBuffer*>(range->GetBufferObject().get());
            return mtlBuffer->GetHandle();
        }
        case IBindableResource::ResourceKind::Texture: {
            auto* texView = common::PtrCast<ITextureView>(resource.get());
            auto* mtlTex =
                common::PtrCast<MetalTexture>(texView->GetTextureObject().get());
            return mtlTex->GetHandle();
        }
        case IBindableResource::ResourceKind::Sampler:
            return nil;
        }
        return nil;
    }

}


MetalCommandList::MetalCommandList(const common::sp<MetalDevice>& dev,
                                   id<MTLCommandBuffer> buffer)
    : _dev(dev)
    , _cmdBuf(buffer)
    , _currEncoder(nullptr)
{

}

MetalCommandList::~MetalCommandList() {
    assert(_currEncoder == nullptr);
    @autoreleasepool {
        //delete _currEncoder;
        [_cmdBuf release];
        //Clear all passes
        for(auto* pass : _passes) {
            delete pass;
        }

        //auto refCnt = [_cmdBuf retainCount];

    }
}


    void MetalCommandList::SetDebugName(const std::string& name) {
        @autoreleasepool {
            auto nsSrc = [NSString stringWithUTF8String:name.c_str()];
            [_cmdBuf setLabel:nsSrc];
        }
    }

    void MetalCommandList::InsertDebugMarker(const std::string& name, const Color4f&) {
        @autoreleasepool {
            id<MTLCommandEncoder> enc;
            if(_currEncoder) {
                enc = _currEncoder->GetEnc();
            } else {
                enc = [_cmdBuf computeCommandEncoder];
            }

            auto nsSrc = [NSString stringWithUTF8String:name.c_str()];
            [enc insertDebugSignpost:nsSrc];

            if(!_currEncoder) {
                [enc endEncoding];
            }
        }
    }


MetalRenderCmdEnc::MetalRenderCmdEnc(
    MetalCommandList* cmdBuf,
    id<MTLRenderCommandEncoder>  enc,
    const RenderPassAction& actions
//RenderPassRegistry& registry
)   : CmdEncoderImplBase(cmdBuf)
    , _mtlEnc(enc)
    , _actions(actions)
   //, registry(registry)
{
    //renderPass->ref();
    //_drawResources.emplace_back();
}


//void MetalCommandList::EncodeWaitForEvent(const common::sp<IEvent>& event, uint64_t expectedValue) {
//    RegisterObjInUse(event);
//    auto mtlEvt = static_cast<EventImpl*>(event.get());
//    @autoreleasepool {
//        [_cmdBuf encodeWaitForEvent:mtlEvt->GetHandle() value:expectedValue];
//    }
//}
//
//void MetalCommandList::EncodeSignalEvent(const common::sp<IEvent>& event, uint64_t value) {
//
//    RegisterObjInUse(event);
//    auto mtlEvt = static_cast<EventImpl*>(event.get());
//    @autoreleasepool {
//        [_cmdBuf encodeSignalEvent: mtlEvt->GetHandle() value:value];
//    }
//}

    MetalRenderCmdEnc::~MetalRenderCmdEnc() {
        //[_mtlEnc release];
    }


    void MetalRenderCmdEnc::SetPipelineBase(MetalGfxPipelineBase* mtlPipeline) {
        _drawResources.boundPipeline = mtlPipeline;
        mtlPipeline->BindToCmdBuf(_mtlEnc);

        auto layout = mtlPipeline->GetPipelineLayout();

        if(layout) {
            auto argBufferSize = layout->GetRootSigSizeInBytes();
            _drawResources.argBuffer.resize(argBufferSize);
        } else {
            _drawResources.argBuffer.clear();
        }
    }

    void MetalRenderCmdEnc::SetPipeline(const common::sp<IGfxPipeline>& pipeline) {
        resources.insert(pipeline);
        auto mtlPipeline = static_cast<MetalGfxPipeline*>(pipeline.get());
        SetPipelineBase(mtlPipeline);
    }


    void MetalRenderCmdEnc::SetPipeline(const common::sp<IMeshShaderPipeline>& pipeline) {
        resources.insert(pipeline);
        auto mtlPipeline = static_cast<MetalMeshShaderPipeline*>(pipeline.get());
        SetPipelineBase(mtlPipeline);
    }


    void MetalRenderCmdEnc::DispatchMesh(std::uint32_t groupCountX,
                                         std::uint32_t groupCountY,
                                         std::uint32_t groupCountZ)
    {
        auto& registry = _drawResources;
        assert(registry.boundPipeline != nullptr);
        assert(IsMeshShaderPipeline(*registry.boundPipeline));

        @autoreleasepool {

            auto mtlPipeline = static_cast<MetalMeshShaderPipeline*>(registry.boundPipeline);


            if(!_drawResources.argBuffer.empty()) {
                auto data = _drawResources.argBuffer.data();
                auto size = _drawResources.argBuffer.size();
                [_mtlEnc setObjectBytes:data
                                 length:size
                                atIndex:kIRArgumentBufferBindPoint];
                [_mtlEnc setMeshBytes:data
                               length:size
                              atIndex:kIRArgumentBufferBindPoint];
                [_mtlEnc setFragmentBytes:data
                                   length:size
                                  atIndex:kIRArgumentBufferBindPoint];
            }

            auto meshShaderWGSize = mtlPipeline->GetMeshShaderWorkgroupSize();
            auto taskShaderWGSize = mtlPipeline->GetTaskShaderWorkgroupSize();

            [_mtlEnc    drawMeshThreadgroups:MTLSizeMake(groupCountX,
                                                         groupCountY,
                                                         groupCountZ)
                 threadsPerObjectThreadgroup:MTLSizeMake(taskShaderWGSize.width,
                                                         taskShaderWGSize.height,
                                                         taskShaderWGSize.depth)
                   threadsPerMeshThreadgroup:MTLSizeMake(meshShaderWGSize.width,
                                                         meshShaderWGSize.height,
                                                         meshShaderWGSize.depth)];
    }
}



void MetalRenderCmdEnc::SetVertexBuffer(std::uint32_t index, const common::sp<BufferRange>& buffer) {
    //TODO: Check max limit of index
    //@autoreleasepool {

        resources.insert(buffer);
        _drawResources.boundVertexBuffers[index] = buffer;
        //auto mtlBuffer = static_cast<MetalBuffer*>(buffer->GetBufferObject());
        //[_mtlEnc setVertexBuffer:mtlBuffer->GetHandle()
        //                  offset:buffer->GetShape().GetOffsetInBytes()
        //                 atIndex:kIRVertexBufferBindPoint + index ];
    //}
}


void MetalRenderCmdEnc::SetIndexBuffer(const common::sp<BufferRange>& buffer, IndexFormat format) {

    resources.insert(buffer);
    _drawResources.boundIndexBuffer = buffer;
    _drawResources.boundIndexBufferFormat = format;

    //Will be use in:
    //[_mtlEnc drawIndexedPrimitives:(MTLPrimitiveType) indexCount:<#(NSUInteger)#> indexType:<#(MTLIndexType)#> indexBuffer:<#(nonnull id<MTLBuffer>)#> indexBufferOffset:<#(NSUInteger)#>]

}

void MetalRenderCmdEnc::SetGraphicsResourceSet(
    const common::sp<IResourceSet>& rs
) {
    assert(_drawResources.boundPipeline != nullptr);

    @autoreleasepool {
        resources.insert(rs);

        auto mtlRS = common::PtrCast<MetalResourceSet>(rs.get());

        auto usedRes = mtlRS->GetUseResources();
        if(!usedRes.empty()) {
            [_mtlEnc useResources:usedRes.data()
                            count:usedRes.size()
                            usage:MTLResourceUsageRead | MTLResourceUsageWrite
                           stages:MTLRenderStageVertex | MTLRenderStageFragment
            ];
        }

        SetRootArgumentBufferHeaps(
            _drawResources.argBuffer,
            mtlRS->GetShaderResHeapGPUVA(),
            mtlRS->GetSamplerHeapGPUVA());

        //[_mtlEnc useResource:sampler usage:MTLResourceUsageRead];
        //[_mtlEnc useResources:(id<MTLResource>  _Nonnull const * _Nonnull) count:(NSUInteger) usage:(MTLResourceUsage) stages:(MTLRenderStages)]

        //[_mtlEnc setVertexBuffer:mtlRS->GetHandle() offset:0 atIndex:kIRArgumentBufferBindPoint];
        //[_mtlEnc setFragmentBuffer:mtlRS->GetHandle() offset:0 atIndex:kIRArgumentBufferBindPoint];
    }
}

void MetalRenderCmdEnc::SetGraphicsMutableResourceSet(
    const common::sp<IMutableResourceSet>& rs
) {
    assert(_drawResources.boundPipeline != nullptr);

    @autoreleasepool {
        resources.insert(rs);

        auto mtlRS = common::PtrCast<MetalMutableResourceSet>(rs.get());

        SetRootArgumentBufferHeaps(
            _drawResources.argBuffer,
            mtlRS->GetShaderResHeapGPUVA(),
            mtlRS->GetSamplerHeapGPUVA());
    }
}

void MetalRenderCmdEnc::SetDescriptorHeaps(
    const common::sp<IResourceDescriptorHeap>& resourceHeap,
    const common::sp<ISamplerDescriptorHeap>& samplerHeap
) {
    @autoreleasepool {
        id<MTLBuffer> resourceHeapBuffer = nil;
        if(resourceHeap) {
            resources.insert(resourceHeap);
            auto* mtlHeap =
                common::PtrCast<MetalResourceDescriptorHeap>(resourceHeap.get());
            resourceHeapBuffer = mtlHeap->GetHandle();
        }

        id<MTLBuffer> samplerHeapBuffer = nil;
        if(samplerHeap) {
            resources.insert(samplerHeap);
            auto* mtlHeap =
                common::PtrCast<MetalSamplerDescriptorHeap>(samplerHeap.get());
            samplerHeapBuffer = mtlHeap->GetHandle();
        }

        auto _SetHeapBuffer = [&](id<MTLBuffer> resourceHeapBuffer, uint32_t index) {

            [_mtlEnc setVertexBuffer:resourceHeapBuffer offset:0 atIndex:index];
            [_mtlEnc setFragmentBuffer:resourceHeapBuffer offset:0 atIndex:index];
            [_mtlEnc setObjectBuffer:resourceHeapBuffer offset:0 atIndex:index];
            [_mtlEnc setMeshBuffer:resourceHeapBuffer offset:0 atIndex:index];
        };

        _SetHeapBuffer(resourceHeapBuffer, kIRDescriptorHeapBindPoint);
        _SetHeapBuffer(samplerHeapBuffer, kIRSamplerHeapBindPoint);
    }
}


void MetalRenderCmdEnc::SetPushConstants(
    std::uint32_t pushConstantIndex,
    std::span<const uint32_t> data,
    std::uint32_t destOffsetIn32BitValues
) {
    @autoreleasepool {
        auto layout = _drawResources.boundPipeline->GetPipelineLayout();
        auto& pcs = layout->GetPushConstants();
        assert(pushConstantIndex < pcs.size());
        auto& pc = pcs[pushConstantIndex];
        assert(pc.sizeInDwords >= destOffsetIn32BitValues + data.size());
        auto offset = pc.offsetInDwords * sizeof(uint32_t);

        memcpy(&_drawResources.argBuffer[offset], data.data(), data.size() * sizeof(uint32_t));
    }
}

void MetalRenderCmdEnc::SetViewports(std::span<const Viewport> viewport) {
    @autoreleasepool {
        std::vector<MTLViewport> mtlVps { };
        mtlVps.reserve(viewport.size());
        for(auto& v : viewport) {
            mtlVps.emplace_back();
            auto& mtlV = mtlVps.back();
            mtlV.originX = v.x;
            mtlV.originY = v.y;
            mtlV.width = v.width;
            mtlV.height = v.height;
            mtlV.zfar = v.maxDepth;
            mtlV.znear = v.minDepth;
        }

        [_mtlEnc setViewports:mtlVps.data() count:mtlVps.size()];
    }
}

void MetalRenderCmdEnc::SetFullViewport() {

    Viewport vp[1];
    bool vpIsSet = false;

    auto _SetVP = [&](const ITexture::Description& texDesc){
        vp[0] = {
            .x = 0,
            .y = 0,
            .width    = (float)texDesc.width,
            .height   = (float)texDesc.height,
            .minDepth = 0,
            .maxDepth = 1,
        };

        vpIsSet = true;
    };

    for(auto& rt : _actions.colorTargetActions) {
        auto& texDesc = rt.target->GetTextureObject()->GetDesc();
        _SetVP(texDesc);
        break;
    }

    if(!vpIsSet) {
        if(_actions.depthTargetAction) {
            auto& dt = _actions.depthTargetAction.value();
            auto& texDesc = dt.target->GetTextureObject()->GetDesc();
            _SetVP(texDesc);
            vpIsSet = true;
        }
    }

    if(!vpIsSet) {
        if(_actions.stencilTargetAction) {
            auto& st = _actions.stencilTargetAction.value();
            auto& texDesc = st.target->GetTextureObject()->GetDesc();
            _SetVP(texDesc);
            vpIsSet = true;
        }
    }

    assert(vpIsSet && "Pass with no render / depth / stencil targets!");

    SetViewports(vp);
}


void MetalRenderCmdEnc::SetScissorRects(std::span<const Rect> rects) {

    std::vector<MTLScissorRect> mtlRects { };
    mtlRects.reserve(rects.size());
    for(auto& r : rects) {
        mtlRects.emplace_back();
        auto& mtlR = mtlRects.back();
        mtlR.x = r.x;
        mtlR.y = r.y;
        mtlR.width = r.width;
        mtlR.height = r.height;
    }

    [_mtlEnc setScissorRects:mtlRects.data() count:mtlRects.size()];
}

void MetalRenderCmdEnc::SetFullScissorRect() {
    Rect sr[1];
    bool srIsSet = false;

    auto _SetSR = [&](const ITexture::Description& texDesc){
        sr[0] = {
            .x = 0,
            .y = 0,
            .width  = texDesc.width,
            .height = texDesc.height,
        };

        srIsSet = true;
    };

    for(auto& rt : _actions.colorTargetActions) {
        auto& texDesc = rt.target->GetTextureObject()->GetDesc();
        _SetSR(texDesc);
        break;
    }

    if(!srIsSet) {
        if(_actions.depthTargetAction) {
            auto& dt = _actions.depthTargetAction.value();
            auto& texDesc = dt.target->GetTextureObject()->GetDesc();
            _SetSR(texDesc);
        }
    }

    if(!srIsSet) {
        if(_actions.stencilTargetAction) {
            auto& st = _actions.stencilTargetAction.value();
            auto& texDesc = st.target->GetTextureObject()->GetDesc();
            _SetSR(texDesc);
        }
    }

    assert(srIsSet && "Pass with no render / depth / stencil targets!");

    SetScissorRects(sr);
}

void MetalRenderCmdEnc::Draw( std::uint32_t vertexCount,
                              std::uint32_t instanceCount,
                              std::uint32_t vertexStart,
                              std::uint32_t instanceStart)
{

    auto& registry = _drawResources;
    assert(registry.boundPipeline != nullptr);
    assert(IsGfxPipeline(*_drawResources.boundPipeline));

    @autoreleasepool {
        auto mtlPipeline = static_cast<MetalGfxPipeline*>(_drawResources.boundPipeline);
        auto vertLayout = mtlPipeline->GetVertexLayouts();

        std::vector<IRRuntimeVertexBuffer> vertArgBuf {vertLayout.size()};
        std::vector<id<MTLResource>> usedRes;

        for(int i = 0; i < vertLayout.size(); i++) {
            auto& vertBuf = registry.boundVertexBuffers[i];
            auto offset = vertBuf->GetShape().GetOffsetInBytes();
            auto mtlBuf = common::PtrCast<MetalBuffer>(vertBuf->GetBufferObject().get());
            auto rawBuf = mtlBuf->GetHandle();
            usedRes.emplace_back(rawBuf);

            auto& thisVertLayout = vertLayout[i];
            auto& thisVertArg = vertArgBuf[i];

            thisVertArg.addr = [rawBuf gpuAddress] + offset;
            thisVertArg.stride = thisVertLayout.stride;
            thisVertArg.length = mtlBuf->GetDesc().sizeInBytes - offset;
        }

        [_mtlEnc useResources:usedRes.data()
                        count:usedRes.size()
                        usage:MTLResourceUsageRead
                       stages:MTLRenderStageVertex];

        [_mtlEnc setVertexBytes:vertArgBuf.data()
                         length:vertArgBuf.size() * sizeof(IRRuntimeVertexBuffer)
                        atIndex:kIRVertexBufferBindPoint];

        if(!_drawResources.argBuffer.empty()) {
            auto data = _drawResources.argBuffer.data();
            auto size = _drawResources.argBuffer.size();
            [_mtlEnc setVertexBytes:data
                             length:size
                            atIndex:kIRArgumentBufferBindPoint];
            [_mtlEnc setFragmentBytes:data
                             length:size
                            atIndex:kIRArgumentBufferBindPoint];
        }

        IRRuntimeDrawPrimitives(_mtlEnc,
                                mtlPipeline->GetPrimitiveTopology(),
                                vertexStart,
                                vertexCount,
                                instanceCount,
                                instanceStart);
    }
}

void MetalRenderCmdEnc::DrawIndexed(
    std::uint32_t indexCount, std::uint32_t instanceCount,
    std::uint32_t indexStart, std::uint32_t vertexOffset,
                              std::uint32_t instanceStart) {

    auto& registry = _drawResources;
    assert(registry.boundPipeline != nullptr);
    assert(registry.boundIndexBuffer != nullptr);

    @autoreleasepool {

        auto mtlPipeline = static_cast<MetalGfxPipeline*>(registry.boundPipeline);

        auto& bufferRange = registry.boundIndexBuffer;
        auto mtlBuffer = PtrCast<MetalBuffer>(bufferRange->GetBufferObject().get());

        auto vertLayout = mtlPipeline->GetVertexLayouts();

        std::vector<IRRuntimeVertexBuffer> vertArgBuf {vertLayout.size()};
        std::vector<id<MTLResource>> usedRes;

        for(int i = 0; i < vertLayout.size(); i++) {
            auto& vertBuf = registry.boundVertexBuffers[i];
            auto offset = vertBuf->GetShape().GetOffsetInBytes();
            auto mtlBuf = PtrCast<MetalBuffer>(vertBuf->GetBufferObject().get());
            auto rawBuf = mtlBuf->GetHandle();
            usedRes.emplace_back(rawBuf);


            auto& thisVertLayout = vertLayout[i];
            auto& thisVertArg = vertArgBuf[i];

            thisVertArg.addr = [rawBuf gpuAddress] + offset;
            thisVertArg.stride = thisVertLayout.stride;
            thisVertArg.length = mtlBuf->GetDesc().sizeInBytes - offset;
        }


        [_mtlEnc useResources:usedRes.data()
                        count:usedRes.size()
                        usage:MTLResourceUsageRead
                       stages:MTLRenderStageVertex];

        [_mtlEnc setVertexBytes:vertArgBuf.data()
                         length:vertArgBuf.size() * sizeof(IRRuntimeVertexBuffer)
                        atIndex:kIRVertexBufferBindPoint];

        if(!_drawResources.argBuffer.empty()) {
            auto data = _drawResources.argBuffer.data();
            auto size = _drawResources.argBuffer.size();
            [_mtlEnc setVertexBytes:data
                             length:size
                            atIndex:kIRArgumentBufferBindPoint];
            [_mtlEnc setFragmentBytes:data
                             length:size
                            atIndex:kIRArgumentBufferBindPoint];
        }


        auto indexElemSize = FormatHelpers::GetSizeInBytes(registry.boundIndexBufferFormat);

        IRRuntimeDrawIndexedPrimitives(_mtlEnc,
                                       mtlPipeline->GetPrimitiveTopology(),
                                       indexCount,
                                       AlToMtlIndexFormat(registry.boundIndexBufferFormat),
                                       mtlBuffer->GetHandle(),
                                       bufferRange->GetShape().GetOffsetInBytes() + indexStart * indexElemSize,
                                       instanceCount,
                                       vertexOffset,
                                       instanceStart);
    }
}


    void MetalComputeCmdEnc::SetPipeline(const common::sp<IComputePipeline>& pipeline) {
        resources.insert(pipeline);
        auto mtlPipeline = static_cast<MetalComputePipeline*>(pipeline.get());
        _compResources.boundPipeline = mtlPipeline;
        mtlPipeline->BindToCmdBuf(_mtlEnc);

        auto layout = mtlPipeline->GetPipelineLayout();

        if(layout) {
            auto argBufferSize = layout->GetRootSigSizeInBytes();
            _compResources.argBuffer.resize(argBufferSize);
        } else {
            _compResources.argBuffer.clear();
        }
    }

    void MetalComputeCmdEnc::SetComputeResourceSet(
        const common::sp<IResourceSet>& rs
    ) {

        assert(_compResources.boundPipeline != nullptr);

        @autoreleasepool {
            resources.insert(rs);

            auto mtlRS = common::PtrCast<MetalResourceSet>(rs.get());

            auto usedRes = mtlRS->GetUseResources();
            if(!usedRes.empty()) {
                [_mtlEnc useResources:usedRes.data()
                            count:usedRes.size()
                            usage:MTLResourceUsageRead | MTLResourceUsageWrite
                ];
            }

            SetRootArgumentBufferHeaps(
                _compResources.argBuffer,
                mtlRS->GetShaderResHeapGPUVA(),
                mtlRS->GetSamplerHeapGPUVA());

            //[_mtlEnc useResource:sampler usage:MTLResourceUsageRead];
            //[_mtlEnc useResources:(id<MTLResource>  _Nonnull const * _Nonnull) count:(NSUInteger) usage:(MTLResourceUsage) stages:(MTLRenderStages)]

            //[_mtlEnc setVertexBuffer:mtlRS->GetHandle() offset:0 atIndex:kIRArgumentBufferBindPoint];
            //[_mtlEnc setFragmentBuffer:mtlRS->GetHandle() offset:0 atIndex:kIRArgumentBufferBindPoint];
        }
    }

    void MetalComputeCmdEnc::SetComputeMutableResourceSet(
        const common::sp<IMutableResourceSet>& rs
    ) {
        assert(_compResources.boundPipeline != nullptr);

        @autoreleasepool {
            resources.insert(rs);

            auto mtlRS = common::PtrCast<MetalMutableResourceSet>(rs.get());

            SetRootArgumentBufferHeaps(
                _compResources.argBuffer,
                mtlRS->GetShaderResHeapGPUVA(),
                mtlRS->GetSamplerHeapGPUVA());
        }
    }

    void MetalComputeCmdEnc::SetDescriptorHeaps(
        const common::sp<IResourceDescriptorHeap>& resourceHeap,
        const common::sp<ISamplerDescriptorHeap>& samplerHeap
    ) {
        @autoreleasepool {
            id<MTLBuffer> resourceHeapBuffer = nil;
            if(resourceHeap) {
                resources.insert(resourceHeap);
                auto* mtlHeap =
                    common::PtrCast<MetalResourceDescriptorHeap>(resourceHeap.get());
                resourceHeapBuffer = mtlHeap->GetHandle();
            }

            id<MTLBuffer> samplerHeapBuffer = nil;
            if(samplerHeap) {
                resources.insert(samplerHeap);
                auto* mtlHeap =
                    common::PtrCast<MetalSamplerDescriptorHeap>(samplerHeap.get());
                samplerHeapBuffer = mtlHeap->GetHandle();
            }

            [_mtlEnc setBuffer:resourceHeapBuffer
                         offset:0
                        atIndex:kIRDescriptorHeapBindPoint];
            [_mtlEnc setBuffer:samplerHeapBuffer
                         offset:0
                        atIndex:kIRSamplerHeapBindPoint];
        }
    }

    void MetalComputeCmdEnc::SetPushConstants(
        std::uint32_t pushConstantIndex,
        std::span<const uint32_t> data,
        std::uint32_t destOffsetIn32BitValues
    ) {
        @autoreleasepool {
            auto layout = _compResources.boundPipeline->GetPipelineLayout();
            auto& pcs = layout->GetPushConstants();
            assert(pushConstantIndex < pcs.size());
            auto& pc = pcs[pushConstantIndex];
            assert(pc.sizeInDwords >= destOffsetIn32BitValues + data.size());
            auto offset = pc.offsetInDwords * sizeof(uint32_t);

            memcpy(&_compResources.argBuffer[offset], data.data(), data.size() * sizeof(uint32_t));
        }
    }


    void MetalComputeCmdEnc::Dispatch(std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ) {

        auto& registry = _compResources;
        assert(registry.boundPipeline != nullptr);

        @autoreleasepool {

            auto mtlPipeline = registry.boundPipeline;

            std::vector<id<MTLResource>> usedRes;

            if(!_compResources.argBuffer.empty()) {
                auto data = _compResources.argBuffer.data();
                auto size = _compResources.argBuffer.size();
                [_mtlEnc setBytes:data
                       length:size
                      atIndex:kIRArgumentBufferBindPoint];
            }

            auto wgSize = mtlPipeline->GetWorkgroupSize();


            [_mtlEnc dispatchThreadgroups:MTLSizeMake(groupCountX, groupCountY, groupCountZ)
                    threadsPerThreadgroup:MTLSizeMake(wgSize.width, wgSize.height, wgSize.depth)];

        }
    }


IRenderCommandEncoder& MetalCommandList::BeginRenderPass(
    const RenderPassAction& action,
    const PassResourceUsage& usage
) {
    assert(_currEncoder == nullptr);

    //RegisterObjInUse(renderPass);

    @autoreleasepool{
        MTLRenderPassDescriptor* pass = [[MTLRenderPassDescriptor new] autorelease];

        for(unsigned i = 0; i < action.colorTargetActions.size(); i++) {
            const auto& ctAct = action.colorTargetActions[i];

            auto* mtlColorTgt = pass.colorAttachments[i];

            mtlColorTgt.clearColor = MTLClearColorMake(ctAct.clearColor.r,
                                                       ctAct.clearColor.g,
                                                       ctAct.clearColor.b,
                                                       ctAct.clearColor.a);

            bool hasMSAATarget = ctAct.msaaResolveTarget != nullptr;

            switch(ctAct.storeAction) {
                case StoreAction::DontCare:
                    mtlColorTgt.storeAction = hasMSAATarget ? MTLStoreActionMultisampleResolve
                                                            : MTLStoreActionDontCare;
                    break;
                case StoreAction::Store:
                    mtlColorTgt.storeAction = hasMSAATarget ? MTLStoreActionStoreAndMultisampleResolve
                                                            : MTLStoreActionStore;
                    break;
            }

            switch(ctAct.loadAction) {
                case LoadAction::DontCare: mtlColorTgt.loadAction = MTLLoadActionDontCare; break;
                case LoadAction::Load: mtlColorTgt.loadAction = MTLLoadActionLoad; break;
                case LoadAction::Clear:mtlColorTgt.loadAction = MTLLoadActionClear; break;
            }

            auto mtlTexView = ctAct.target;
            auto mtlColorTex = PtrCast<MetalTexture>(mtlTexView->GetTextureObject().get());
            mtlColorTgt.texture = mtlColorTex->GetHandle();
            mtlColorTgt.level = mtlTexView->GetDesc().baseMipLevel;
            mtlColorTgt.slice = mtlTexView->GetDesc().baseArrayLayer;

            if(hasMSAATarget) {
                auto mtlResolveTexView = ctAct.msaaResolveTarget;
                auto mtlResolveTex = PtrCast<MetalTexture>(mtlResolveTexView->GetTextureObject().get());
                mtlColorTgt.resolveTexture = mtlResolveTex->GetHandle();
                mtlColorTgt.resolveLevel = mtlResolveTexView->GetDesc().baseMipLevel;
                mtlColorTgt.resolveSlice = mtlResolveTexView->GetDesc().baseArrayLayer;
            }
        }

        if(action.depthTargetAction) {

            const auto& dtAct = action.depthTargetAction.value();

            bool hasMSAATarget = dtAct.msaaResolveTarget != nullptr;

            auto* mtlDT = pass.depthAttachment;
            mtlDT.clearDepth = dtAct.clearDepth;

            switch(action.depthTargetAction->storeAction) {
                case StoreAction::DontCare:
                    mtlDT.storeAction = hasMSAATarget ? MTLStoreActionMultisampleResolve
                                                      : MTLStoreActionDontCare;
                    break;
                case StoreAction::Store:
                    mtlDT.storeAction = hasMSAATarget ? MTLStoreActionStoreAndMultisampleResolve
                                                      : MTLStoreActionStore;
                    break;
            }

            switch(action.depthTargetAction->loadAction) {
                case LoadAction::DontCare: mtlDT.loadAction = MTLLoadActionDontCare; break;
                case LoadAction::Load: mtlDT.loadAction = MTLLoadActionLoad; break;
                case LoadAction::Clear:mtlDT.loadAction = MTLLoadActionClear; break;
            }

            auto mtlTexView = dtAct.target;
            auto mtlDepthTex = PtrCast<MetalTexture>(mtlTexView->GetTextureObject().get());

            mtlDT.texture = mtlDepthTex->GetHandle();
            mtlDT.level = mtlTexView->GetDesc().baseMipLevel;
            mtlDT.slice = mtlTexView->GetDesc().baseArrayLayer;

            if(hasMSAATarget) {
                auto& mtlResolveTexView = dtAct.msaaResolveTarget;
                auto mtlResolveTex = PtrCast<MetalTexture>(mtlResolveTexView->GetTextureObject().get());
                mtlDT.resolveTexture = mtlResolveTex->GetHandle();
                mtlDT.resolveLevel = mtlResolveTexView->GetDesc().baseMipLevel;
                mtlDT.resolveSlice = mtlResolveTexView->GetDesc().baseArrayLayer;

                switch(dtAct.msaaResolveMode) {
                    case MSAADepthResolveMode::Min : mtlDT.depthResolveFilter = MTLMultisampleDepthResolveFilterMin; break;
                    case MSAADepthResolveMode::Max : mtlDT.depthResolveFilter = MTLMultisampleDepthResolveFilterMax; break;
                }
            }

        }

        if(action.stencilTargetAction) {
            auto* mtlST = pass.stencilAttachment;
            mtlST.clearStencil = action.stencilTargetAction->clearStencil;

            switch(action.stencilTargetAction->storeAction) {
                case StoreAction::DontCare: mtlST.storeAction = MTLStoreActionDontCare; break;
                case StoreAction::Store: mtlST.storeAction = MTLStoreActionStore; break;
            }

            switch(action.stencilTargetAction->loadAction) {
                case LoadAction::DontCare: mtlST.loadAction = MTLLoadActionDontCare; break;
                case LoadAction::Load: mtlST.loadAction = MTLLoadActionLoad; break;
                case LoadAction::Clear:mtlST.loadAction = MTLLoadActionClear; break;
            }

            auto mtlTex = common::SPCast<MetalTexture>(action.stencilTargetAction->target->GetTextureObject());
            mtlST.texture = mtlTex->GetHandle();
            mtlST.level = action.stencilTargetAction->target->GetDesc().baseMipLevel;
            mtlST.slice = action.stencilTargetAction->target->GetDesc().baseArrayLayer;
        }

        //RenderPassImpl* mtlRndrPass = static_cast<RenderPassImpl*>(renderPass.get());
        id<MTLRenderCommandEncoder> rawEnc
            = [[_cmdBuf renderCommandEncoderWithDescriptor: pass] retain];

        MetalRenderCmdEnc* thisEncoder = new MetalRenderCmdEnc(this, rawEnc, action);
        for(auto& resourceUsage : usage) {
            auto resource = GetMetalResource(resourceUsage.resource);
            if(!resource) {
                continue;
            }

            _objsInUse.insert(resourceUsage.resource);
            [rawEnc useResource:resource
                           usage:ToMTLResourceUsage(resourceUsage.access)
                          stages:ToMTLRenderStages(resourceUsage.stages)];
        }

        _passes.push_back(thisEncoder);
        _currEncoder = thisEncoder;
        return *thisEncoder;
    }

}



    IComputeCommandEncoder&  MetalCommandList::BeginComputePass(
        const PassResourceUsage& usage){
        assert(_currEncoder == nullptr);

        @autoreleasepool {
            MTLComputePassDescriptor* pass = [[MTLComputePassDescriptor new] autorelease];
            //pass.dispatchType = MTLDispatchTypeSerial;

            id<MTLComputeCommandEncoder> rawEnc
                = [[_cmdBuf computeCommandEncoderWithDescriptor:pass] retain];

            auto thisEncoder = new MetalComputeCmdEnc(this, rawEnc);
            for(auto& resourceUsage : usage) {
                auto resource = GetMetalResource(resourceUsage.resource);
                if(!resource) {
                    continue;
                }

                _objsInUse.insert(resourceUsage.resource);
                [rawEnc useResource:resource
                              usage:ToMTLResourceUsage(resourceUsage.access)];
            }

            _passes.push_back(thisEncoder);
            _currEncoder = thisEncoder;
            return *thisEncoder;
        }
    }


    ITransferCommandEncoder& MetalCommandList::BeginTransferPass(){

        assert(_currEncoder == nullptr);

        @autoreleasepool {
            MTLBlitPassDescriptor* pass = [[MTLBlitPassDescriptor new] autorelease];

            id<MTLBlitCommandEncoder> rawEnc
                = [[_cmdBuf blitCommandEncoderWithDescriptor:pass] retain];

            auto thisEncoder = new MetalTransferCmdEnc(this, rawEnc);

            _passes.push_back(thisEncoder);
            _currEncoder = thisEncoder;
            return *thisEncoder;
        }

    }

void MetalTransferCmdEnc::CopyBuffer(
    const common::sp<BufferRange>& source,
    const common::sp<BufferRange>& destination,
    std::uint32_t sizeInBytes
) {
    assert(source->GetShape().GetSizeInBytes() >= sizeInBytes);
    assert(destination->GetShape().GetSizeInBytes() >= sizeInBytes);

    resources.insert(source);
    resources.insert(destination);
    auto srcBufferImpl = PtrCast<MetalBuffer>(source->GetBufferObject().get());
    auto dstBufferImpl = PtrCast<MetalBuffer>(destination->GetBufferObject().get());
    [_mtlEnc    copyFromBuffer:srcBufferImpl->GetHandle()
                  sourceOffset:source->GetShape().GetOffsetInBytes()
                      toBuffer:dstBufferImpl->GetHandle()
             destinationOffset:destination->GetShape().GetOffsetInBytes()
                          size:sizeInBytes
    ];
}



    //TODO: should we adjust to resource views?
    void MetalTransferCmdEnc::CopyBufferToTexture(
        const common::sp<BufferRange>& source,
        std::uint32_t sourceBytesPerRow,
        std::uint32_t sourceBytesPerImage,
        const common::sp<ITextureView>& destination,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ) {


        resources.insert(source);
        resources.insert(destination);

        auto mtlBufSrc = common::PtrCast<MetalBuffer>(source->GetBufferObject().get());
        auto mtlTexDst = PtrCast<MetalTexture>(destination->GetTextureObject().get());
        const auto& texViewDesc = destination->GetDesc();

        [_mtlEnc   copyFromBuffer:(id<MTLBuffer>)mtlBufSrc->GetHandle()
                        sourceOffset:(NSUInteger)source->GetShape().GetOffsetInBytes()
                sourceBytesPerRow:(NSUInteger)sourceBytesPerRow
                sourceBytesPerImage:(NSUInteger)sourceBytesPerImage
                        sourceSize:(MTLSize)MTLSizeMake(copySize.width, copySize.height, copySize.depth)
                        toTexture:(id<MTLTexture>)mtlTexDst->GetHandle()
                    destinationSlice:(NSUInteger)texViewDesc.baseArrayLayer + dstBaseArrayLayer
                    destinationLevel:(NSUInteger)texViewDesc.baseMipLevel + dstMipLevel
                destinationOrigin:(MTLOrigin)MTLOriginMake(dstOrigin.x, dstOrigin.y, dstOrigin.z)
        ];
    }

    void MetalTransferCmdEnc::CopyTextureToBuffer(
        const common::sp<ITextureView>& source,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<BufferRange>& destination,
        std::uint32_t dstBytesPerRow,
        std::uint32_t dstBytesPerImage,
        const Size3D& copySize
    ) {

        resources.insert(source);
        resources.insert(destination);

        auto mtlTexSrc = PtrCast<MetalTexture>(source->GetTextureObject().get());
        const auto& texViewDesc = source->GetDesc();

        auto mtlBufDst = PtrCast<MetalBuffer>(destination->GetBufferObject().get());

        [_mtlEnc    copyFromTexture:(id<MTLTexture>)mtlTexSrc->GetHandle()
                        sourceSlice:(NSUInteger)texViewDesc.baseArrayLayer + srcBaseArrayLayer
                        sourceLevel:(NSUInteger)texViewDesc.baseMipLevel + srcMipLevel
                       sourceOrigin:(MTLOrigin)MTLOriginMake(srcOrigin.x, srcOrigin.y, srcOrigin.z)
                         sourceSize:(MTLSize)MTLSizeMake(copySize.width, copySize.height, copySize.depth)
                           toBuffer:(id<MTLBuffer>)mtlBufDst->GetHandle()
                  destinationOffset:(NSUInteger)destination->GetShape().GetOffsetInBytes()
             destinationBytesPerRow:(NSUInteger)dstBytesPerRow
           destinationBytesPerImage:(NSUInteger)dstBytesPerImage
        ];
    }
        /// Copies a region from one <see cref="Texture"/> into another.
        /// <param name="source">The source <see cref="Texture"/> from which data is copied.</param>
        /// <param name="srcX">The X coordinate of the source copy region.</param>
        /// <param name="srcY">The Y coordinate of the source copy region.</param>
        /// <param name="srcZ">The Z coordinate of the source copy region.</param>
        /// <param name="srcMipLevel">The mip level to copy from the source Texture.</param>
        /// <param name="srcBaseArrayLayer">The starting array layer to copy from the source Texture.</param>
        /// <param name="destination">The destination <see cref="Texture"/> into which data is copied.</param>
        /// <param name="dstX">The X coordinate of the destination copy region.</param>
        /// <param name="dstY">The Y coordinate of the destination copy region.</param>
        /// <param name="dstZ">The Z coordinate of the destination copy region.</param>
        /// <param name="dstMipLevel">The mip level to copy the data into.</param>
        /// <param name="dstBaseArrayLayer">The starting array layer to copy data into.</param>
        /// <param name="width">The width in texels of the copy region.</param>
        /// <param name="height">The height in texels of the copy region.</param>
        /// <param name="depth">The depth in texels of the copy region.</param>
        /// <param name="layerCount">The number of array layers to copy.</param>
    void MetalTransferCmdEnc::CopyTexture(
        const common::sp<ITextureView>& source,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<ITextureView>& destination,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize)
    {

        resources.insert(source);
        resources.insert(destination);

        auto mtlTexSrc = PtrCast<MetalTexture>(source->GetTextureObject().get());
        const auto& srcTexViewDesc = source->GetDesc();

        auto mtlTexDst = PtrCast<MetalTexture>(destination->GetTextureObject().get());
        const auto& dstTexViewDesc = destination->GetDesc();

        [_mtlEnc copyFromTexture:mtlTexSrc->GetHandle()
                     sourceSlice:(NSUInteger)srcTexViewDesc.baseArrayLayer + srcBaseArrayLayer
                     sourceLevel:(NSUInteger)srcTexViewDesc.baseMipLevel + srcMipLevel
                    sourceOrigin:(MTLOrigin)MTLOriginMake(srcOrigin.x, srcOrigin.y, srcOrigin.z)
                      sourceSize:(MTLSize)MTLSizeMake(copySize.width, copySize.height, copySize.depth)
                       toTexture:mtlTexDst->GetHandle()
                destinationSlice:(NSUInteger)dstTexViewDesc.baseArrayLayer + dstBaseArrayLayer
                destinationLevel:(NSUInteger)dstTexViewDesc.baseMipLevel + dstMipLevel
               destinationOrigin:(MTLOrigin)MTLOriginMake(dstOrigin.x, dstOrigin.y, dstOrigin.z)
        ];

    }

        /// <summary>
        /// Copies all subresources from one <see cref="Texture"/> to another.
        /// </summary>
        /// <param name="source">The source of Texture data.</param>
        /// <param name="destination">The destination of Texture data.</param>
/*        void CopyTexture(const common::sp<Texture>& source, const common::sp<Texture>& destination)
        {
            auto& desc = source->GetDesc();

            std::uint32_t effectiveSrcArrayLayers = (desc.usage.cubemap) != 0
                ? desc.arrayLayers * 6
                : desc.arrayLayers;
#if VALIDATE_USAGE
            std::uint32_t effectiveDstArrayLayers = (destination.Usage & TextureUsage.Cubemap) != 0
                ? destination.ArrayLayers * 6
                : destination.ArrayLayers;
            if (effectiveSrcArrayLayers != effectiveDstArrayLayers || source.MipLevels != destination.MipLevels
                || source.SampleCount != destination.SampleCount || source.Width != destination.Width
                || source.Height != destination.Height || source.Depth != destination.Depth
                || source.Format != destination.Format)
            {
                throw new VeldridException("Source and destination Textures are not compatible to be copied.");
            }
#endif

            for (std::uint32_t level = 0; level < source->GetDesc().mipLevels; level++)
            {
                std::uint32_t mipWidth, mipHeight, mipDepth;

                Helpers::GetMipDimensions(source->GetDesc(), level, mipWidth, mipHeight, mipDepth);
                CopyTexture(
                    source, 0, 0, 0, level, 0,
                    destination, 0, 0, 0, level, 0,
                    mipWidth, mipHeight, mipDepth,
                    effectiveSrcArrayLayers);
            }
        }*/

/*
        void CopyTexture(
            const common::sp<Texture>& source, const common::sp<Texture>& destination,
            std::uint32_t mipLevel, std::uint32_t arrayLayer)
        {
#if VALIDATE_USAGE
            std::uint32_t effectiveSrcArrayLayers = (source.Usage & TextureUsage.Cubemap) != 0
                ? source.ArrayLayers * 6
                : source.ArrayLayers;
            std::uint32_t effectiveDstArrayLayers = (destination.Usage & TextureUsage.Cubemap) != 0
                ? destination.ArrayLayers * 6
                : destination.ArrayLayers;
            if (source.SampleCount != destination.SampleCount || source.Width != destination.Width
                || source.Height != destination.Height || source.Depth != destination.Depth
                || source.Format != destination.Format)
            {
                throw new VeldridException("Source and destination Textures are not compatible to be copied.");
            }
            if (mipLevel >= source.MipLevels || mipLevel >= destination.MipLevels || arrayLayer >= effectiveSrcArrayLayers || arrayLayer >= effectiveDstArrayLayers)
            {
                throw new VeldridException(
                    $"{nameof(mipLevel)} and {nameof(arrayLayer)} must be less than the given Textures' mip level count and array layer count.");
            }
#endif
            std::uint32_t width, height, depth;
            Helpers::GetMipDimensions(source->GetDesc(), mipLevel, width, height, depth);
            CopyTexture(
                source, 0, 0, 0, mipLevel, arrayLayer,
                destination, 0, 0, 0, mipLevel, arrayLayer,
                width, height, depth,
                1);
        }*/

        // Generates mipmaps for the given <see cref="Texture"/>. The largest mipmap is used to generate all of the lower mipmap
        // levels contained in the Texture. The previous contents of all lower mipmap levels are overwritten by this operation.
        // The target Texture must have been created with <see cref="TextureUsage"/>.<see cref="TextureUsage.GenerateMipmaps"/>.
        // <param name="texture">The <see cref="Texture"/> to generate mipmaps for. This Texture must have been created with
        // <see cref="TextureUsage"/>.<see cref="TextureUsage.GenerateMipmaps"/>.</param>
    void MetalTransferCmdEnc::GenerateMipmaps(const common::sp<ITexture>& texture) {
        resources.insert(texture);
        auto texImpl = static_cast<MetalTexture*>(texture.get());

        [_mtlEnc generateMipmapsForTexture:texImpl->GetHandle()];
    }

} // namespace alloy::mtl
