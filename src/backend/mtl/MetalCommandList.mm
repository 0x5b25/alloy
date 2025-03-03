#include "MetalCommandList.h"
#include "MetalDevice.h"
#include "MetalTexture.h"
#include "MetalBindableResource.h"
#include "MtlTypeCvt.h"
#include "alloy/RenderPass.hpp"
#include "MetalPipeline.h"

#include <Metal/MTLTypes.h>
#include <Metal/Metal.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include <cassert>

namespace alloy::mtl {


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

void MetalRenderCmdEnc::SetPipeline(const common::sp<IGfxPipeline>& pipeline) {
    resources.insert(pipeline);
    auto mtlPipeline = static_cast<MetalGfxPipeline*>(pipeline.get());
    _drawResources.boundPipeline = common::ref_sp(mtlPipeline);
    mtlPipeline->BindToCmdBuf(_mtlEnc);

    auto layout = mtlPipeline->GetPipelineLayout();
    
    if(layout) {
        auto argBufferSize = layout->GetRootSigSizeInBytes();
        _drawResources.argBuffer.resize(argBufferSize);
    } else {
        _drawResources.argBuffer.clear();
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
        [_mtlEnc useResources:usedRes.data()
                        count:usedRes.size()
                        usage:MTLResourceUsageRead | MTLResourceUsageWrite
                       stages:MTLRenderStageVertex | MTLRenderStageFragment
        ];

        id<MTLSamplerState> sampler;
        
        auto layout = _drawResources.boundPipeline->GetPipelineLayout();

        auto pHeap =(uint64_t*)_drawResources.argBuffer.data();
        auto shResVA = mtlRS->GetShaderResHeapGPUVA();
        auto sampVA = mtlRS->GetSamplerHeapGPUVA();
        if(shResVA) {
            *pHeap = shResVA;
            pHeap++;
        }
        if(sampVA) {
            *pHeap = sampVA;
            pHeap++;
        }

        //[_mtlEnc useResource:sampler usage:MTLResourceUsageRead];
        //[_mtlEnc useResources:(id<MTLResource>  _Nonnull const * _Nonnull) count:(NSUInteger) usage:(MTLResourceUsage) stages:(MTLRenderStages)]
        
        //[_mtlEnc setVertexBuffer:mtlRS->GetHandle() offset:0 atIndex:kIRArgumentBufferBindPoint];
        //[_mtlEnc setFragmentBuffer:mtlRS->GetHandle() offset:0 atIndex:kIRArgumentBufferBindPoint];
    }
}


void MetalRenderCmdEnc::SetPushConstants(
    std::uint32_t pushConstantIndex,
    std::uint32_t num32BitValuesToSet,
    const uint32_t* pSrcData,
    std::uint32_t destOffsetIn32BitValues
) {
    @autoreleasepool {
        auto layout = _drawResources.boundPipeline->GetPipelineLayout();
        auto& pcs = layout->GetPushConstants();
        assert(pushConstantIndex < pcs.size());
        auto& pc = pcs[pushConstantIndex];
        assert(pc.sizeInDwords >= destOffsetIn32BitValues + num32BitValuesToSet);
        auto offset = pc.offsetInDwords * sizeof(uint32_t);

        memcpy(&_drawResources.argBuffer[offset], pSrcData, num32BitValuesToSet * sizeof(uint32_t));
    }
}

void MetalRenderCmdEnc::SetViewports(const std::span<Viewport>& viewport) {
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

void MetalRenderCmdEnc::SetFullViewports() {
    std::vector<Viewport> vps;
    for (auto& a : _actions.colorTargetActions) {
        auto& desc = a.target->GetTexture().GetTextureObject()->GetDesc();
        Viewport vp {
            .x = 0,
            .y = 0,
            .width = (float)desc.width,
            .height = (float)desc.height,
            .minDepth = 0,
            .maxDepth = (float)desc.depth
        };
        
        vps.push_back(vp);
    }
    
    SetViewports(vps);
}


void MetalRenderCmdEnc::SetScissorRects(const std::span<Rect>& rects) {
    
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

void MetalRenderCmdEnc::SetFullScissorRects() {
    std::vector<Rect> rects;
    for (auto& a : _actions.colorTargetActions) {
        auto& desc = a.target->GetTexture().GetTextureObject()->GetDesc();
        Rect rect {
            .x = 0,
            .y = 0,
            .width = desc.width,
            .height = desc.height,
        };
        
        rects.push_back(rect);
    }
    
    SetScissorRects(rects);
}

void MetalRenderCmdEnc::Draw( std::uint32_t vertexCount,
                                     std::uint32_t instanceCount,
                                     std::uint32_t vertexStart, 
                                     std::uint32_t instanceStart) {
    assert(_drawResources.boundPipeline != nullptr);
    //assert(registry.boundIndexBuffer != nullptr);
    
    @autoreleasepool {

        auto mtlPipeline = static_cast<MetalGfxPipeline*>(_drawResources.boundPipeline.get());

        //auto& bufferRange = registry.boundIndexBuffer;
        //auto mtlBuffer = static_cast<AllocationImpl*>(bufferRange->GetBufferObject());
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

        //[_mtlEnc drawPrimitives:mtlPipeline->GetPrimitiveTopology() 
        //            vertexStart:vertexStart
        //            vertexCount:vertexCount
        //          instanceCount:instanceCount
        //           baseInstance:instanceStart];
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

        auto mtlPipeline = static_cast<MetalGfxPipeline*>(registry.boundPipeline.get());

        auto& bufferRange = registry.boundIndexBuffer;
        auto mtlBuffer = static_cast<MetalBuffer*>(bufferRange->GetBufferObject());
        
        auto vertLayout = mtlPipeline->GetVertexLayouts();
        
        std::vector<IRRuntimeVertexBuffer> vertArgBuf {vertLayout.size()};
        std::vector<id<MTLResource>> usedRes;
        
        for(int i = 0; i < vertLayout.size(); i++) {
            auto& vertBuf = registry.boundVertexBuffers[i];
            auto offset = vertBuf->GetShape().GetOffsetInBytes();
            auto mtlBuf = common::PtrCast<MetalBuffer>(vertBuf->GetBufferObject());
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

        //[_mtlEnc drawIndexedPrimitives:mtlPipeline->GetPrimitiveTopology()
        //         indexCount:indexCount
        //         indexType: AlToMtlIndexFormat(registry.boundIndexBufferFormat)
        //         indexBuffer:mtlBuffer->GetHandle()
        //         indexBufferOffset:bufferRange->GetShape().GetOffsetInBytes()
        //         instanceCount:instanceCount
        //         baseVertex:vertexOffset
        //         baseInstance:instanceStart];
    }
}


void MetalComputeCmdEnc::SetPipeline(const common::sp<IComputePipeline>&) {
    
}

void MetalComputeCmdEnc::SetComputeResourceSet(
    const common::sp<IResourceSet>& rs
/*const std::vector<std::uint32_t>& dynamicOffsets*/) {
    
}


void MetalComputeCmdEnc::SetPushConstants(
    std::uint32_t pushConstantIndex,
    std::uint32_t num32BitValuesToSet,
    const uint32_t* pSrcData,
    std::uint32_t destOffsetIn32BitValues
                                          ) {
    
}


/// <summary>
/// Dispatches a compute operation from the currently-bound compute state of this Pipeline.
/// </summary>
/// <param name="groupCountX">The X dimension of the compute thread groups that are dispatched.</param>
/// <param name="groupCountY">The Y dimension of the compute thread groups that are dispatched.</param>
/// <param name="groupCountZ">The Z dimension of the compute thread groups that are dispatched.</param>
void MetalComputeCmdEnc::Dispatch(std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ) {
    
}


IRenderCommandEncoder& MetalCommandList::BeginRenderPass(const RenderPassAction& action)
{
    assert(_currEncoder == nullptr);

    //RegisterObjInUse(renderPass);

    @autoreleasepool{
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];

        for(unsigned i = 0; i < action.colorTargetActions.size(); i++) {
            const auto& colorTgt = action.colorTargetActions[i];

            auto* mtlColorTgt = pass.colorAttachments[i];

            mtlColorTgt.clearColor = MTLClearColorMake(colorTgt.clearColor.r, 
                                                     colorTgt.clearColor.g,
                                                      colorTgt.clearColor.b,
                                                     colorTgt.clearColor.a);
            switch(colorTgt.storeAction) {
            case StoreAction::DontCare: mtlColorTgt.storeAction = MTLStoreActionDontCare; break;
            case StoreAction::Store: mtlColorTgt.storeAction = MTLStoreActionStore; break;
            case StoreAction::MultisampleResolve: mtlColorTgt.storeAction = MTLStoreActionMultisampleResolve; break;
            case StoreAction::StoreAndMultisampleResolve: mtlColorTgt.storeAction = MTLStoreActionStoreAndMultisampleResolve; break;
            case StoreAction::CustomSampleDepthStore: mtlColorTgt.storeAction = MTLStoreActionCustomSampleDepthStore; break;
            }

            switch(colorTgt.loadAction) {
            case LoadAction::DontCare: mtlColorTgt.loadAction = MTLLoadActionDontCare; break;
            case LoadAction::Load: mtlColorTgt.loadAction = MTLLoadActionLoad; break;
            case LoadAction::Clear:mtlColorTgt.loadAction = MTLLoadActionClear; break;
            }

            auto mtlTex = common::SPCast<MetalTexture>(colorTgt.target->GetTexture().GetTextureObject());
            mtlColorTgt.texture = mtlTex->GetHandle();
            mtlColorTgt.level = colorTgt.target->GetTexture().GetDesc().baseMipLevel;
            mtlColorTgt.slice = colorTgt.target->GetTexture().GetDesc().baseArrayLayer;


        }

        if(action.depthTargetAction) {

            auto* mtlDT = pass.depthAttachment;
            mtlDT.clearDepth = action.depthTargetAction->clearDepth;
            
            switch(action.depthTargetAction->storeAction) {
            case StoreAction::DontCare: mtlDT.storeAction = MTLStoreActionDontCare; break;
            case StoreAction::Store: mtlDT.storeAction = MTLStoreActionStore; break;
            case StoreAction::MultisampleResolve: mtlDT.storeAction = MTLStoreActionMultisampleResolve; break;
            case StoreAction::StoreAndMultisampleResolve: mtlDT.storeAction = MTLStoreActionStoreAndMultisampleResolve; break;
            case StoreAction::CustomSampleDepthStore: mtlDT.storeAction = MTLStoreActionCustomSampleDepthStore; break;
            }

            switch(action.depthTargetAction->loadAction) {
            case LoadAction::DontCare: mtlDT.loadAction = MTLLoadActionDontCare; break;
            case LoadAction::Load: mtlDT.loadAction = MTLLoadActionLoad; break;
            case LoadAction::Clear:mtlDT.loadAction = MTLLoadActionClear; break;
            }

            auto mtlTex = common::SPCast<MetalTexture>(action.depthTargetAction->target->GetTexture().GetTextureObject());
            mtlDT.texture = mtlTex->GetHandle();
            mtlDT.level = action.depthTargetAction->target->GetTexture().GetDesc().baseMipLevel;
            mtlDT.slice = action.depthTargetAction->target->GetTexture().GetDesc().baseArrayLayer;

        }

        if(action.stencilTargetAction) {
            auto* mtlST = pass.stencilAttachment;
            mtlST.clearStencil = action.stencilTargetAction->clearStencil;
            
            switch(action.stencilTargetAction->storeAction) {
            case StoreAction::DontCare: mtlST.storeAction = MTLStoreActionDontCare; break;
            case StoreAction::Store: mtlST.storeAction = MTLStoreActionStore; break;
            case StoreAction::MultisampleResolve: mtlST.storeAction = MTLStoreActionMultisampleResolve; break;
            case StoreAction::StoreAndMultisampleResolve: mtlST.storeAction = MTLStoreActionStoreAndMultisampleResolve; break;
            case StoreAction::CustomSampleDepthStore: mtlST.storeAction = MTLStoreActionCustomSampleDepthStore; break;
            }

            switch(action.stencilTargetAction->loadAction) {
            case LoadAction::DontCare: mtlST.loadAction = MTLLoadActionDontCare; break;
            case LoadAction::Load: mtlST.loadAction = MTLLoadActionLoad; break;
            case LoadAction::Clear:mtlST.loadAction = MTLLoadActionClear; break;
            }

            auto mtlTex = common::SPCast<MetalTexture>(action.stencilTargetAction->target->GetTexture().GetTextureObject());
            mtlST.texture = mtlTex->GetHandle();
            mtlST.level = action.stencilTargetAction->target->GetTexture().GetDesc().baseMipLevel;
            mtlST.slice = action.stencilTargetAction->target->GetTexture().GetDesc().baseArrayLayer;
        }
    
        //RenderPassImpl* mtlRndrPass = static_cast<RenderPassImpl*>(renderPass.get());
        id<MTLRenderCommandEncoder> rawEnc
            = [[_cmdBuf renderCommandEncoderWithDescriptor: pass] retain];
    
        MetalRenderCmdEnc* thisEncoder = new MetalRenderCmdEnc(this, rawEnc, action);
        
        _passes.push_back(thisEncoder);
        _currEncoder = thisEncoder;
        return *thisEncoder;
    }
    
}



    IComputeCommandEncoder&  MetalCommandList::BeginComputePass(){
        assert(_currEncoder == nullptr);

        @autoreleasepool {
            MTLComputePassDescriptor* pass = [MTLComputePassDescriptor computePassDescriptor];
            //pass.dispatchType = MTLDispatchTypeSerial;
            
            id<MTLComputeCommandEncoder> rawEnc
                = [[_cmdBuf computeCommandEncoderWithDescriptor:pass] retain];
        
            auto thisEncoder = new MetalComputeCmdEnc(this, rawEnc);

            _passes.push_back(thisEncoder);
            _currEncoder = thisEncoder;
            return *thisEncoder;
        }
    }


    ITransferCommandEncoder& MetalCommandList::BeginTransferPass(){
        
        assert(_currEncoder == nullptr);

        @autoreleasepool {
            MTLBlitPassDescriptor* pass = [MTLBlitPassDescriptor blitPassDescriptor];

            id<MTLBlitCommandEncoder> rawEnc
                = [[_cmdBuf blitCommandEncoderWithDescriptor:pass] retain];
        
            auto thisEncoder = new MetalTransferCmdEnc(this, rawEnc);

            _passes.push_back(thisEncoder);
            _currEncoder = thisEncoder;
            return *thisEncoder;
        }

    }


void MetalRenderCmdEnc::WaitForFenceBeforeStages(const common::sp<IFence>& fence, const PipelineStages& stages) {
#if 0   
    resources.push_back(fence);

    auto fenceImpl = static_cast<FenceImpl*>(fence.get());

    MTLRenderStages mtlStages;

    if(stages[RenderStage::Vertex]) mtlStages |= MTLRenderStageVertex;
    if(stages[RenderStage::Fragment]) mtlStages |= MTLRenderStageFragment;
    if(stages[RenderStage::Mesh]) mtlStages |= MTLRenderStageMesh;
    if(stages[RenderStage::Object]) mtlStages |= MTLRenderStageObject;
    if(stages[RenderStage::Tile]) mtlStages |= MTLRenderStageTile;

    [_mtlEnc waitForFence:fenceImpl->GetHandle() beforeStages:mtlStages ];
#endif
}
void MetalRenderCmdEnc::UpdateFenceAfterStages(const common::sp<IFence>& fence, const PipelineStages& stages) {
#if 0
    RegisterObjInUse(fence);

    auto fenceImpl = static_cast<FenceImpl*>(fence.get());

    MTLRenderStages mtlStages;

    if(stages[RenderStage::Vertex]) mtlStages |= MTLRenderStageVertex;
    if(stages[RenderStage::Fragment]) mtlStages |= MTLRenderStageFragment;
    if(stages[RenderStage::Mesh]) mtlStages |= MTLRenderStageMesh;
    if(stages[RenderStage::Object]) mtlStages |= MTLRenderStageObject;
    if(stages[RenderStage::Tile]) mtlStages |= MTLRenderStageTile;

    [_mtlEnc updateFence:fenceImpl->GetHandle() afterStages:mtlStages ];
#endif
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
    auto srcBufferImpl = static_cast<MetalBuffer*>(source->GetBufferObject());
    auto dstBufferImpl = static_cast<MetalBuffer*>(destination->GetBufferObject());
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
        const common::sp<ITexture>& destination,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ) {

        
        resources.insert(source);
        resources.insert(destination);

        auto mtlBufSrc = static_cast<MetalBuffer*>(source->GetBufferObject());
        auto mtlTexDst = static_cast<MetalTexture*>(destination.get());

        [_mtlEnc   copyFromBuffer:(id<MTLBuffer>)mtlBufSrc->GetHandle() 
                        sourceOffset:(NSUInteger)source->GetShape().GetOffsetInBytes() 
                sourceBytesPerRow:(NSUInteger)sourceBytesPerRow 
                sourceBytesPerImage:(NSUInteger)sourceBytesPerImage 
                        sourceSize:(MTLSize)MTLSizeMake(copySize.width, copySize.height, copySize.depth)  
                        toTexture:(id<MTLTexture>)mtlTexDst->GetHandle() 
                    destinationSlice:(NSUInteger)dstBaseArrayLayer 
                    destinationLevel:(NSUInteger)dstMipLevel 
                destinationOrigin:(MTLOrigin)MTLOriginMake(dstOrigin.x, dstOrigin.y, dstOrigin.z) 
        ];
    }

    void MetalTransferCmdEnc::CopyTextureToBuffer(
        const common::sp<ITexture>& source,
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

        auto mtlTexSrc = static_cast<MetalTexture*>(source.get());
        auto mtlBufDst = static_cast<MetalBuffer*>(destination->GetBufferObject());

        [_mtlEnc    copyFromTexture:(id<MTLTexture>)mtlTexSrc->GetHandle() 
                        sourceSlice:(NSUInteger)srcBaseArrayLayer 
                        sourceLevel:(NSUInteger)srcMipLevel 
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
        const common::sp<ITexture>& source,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<ITexture>& destination,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize)
    {

        resources.insert(source);
        resources.insert(destination);

        auto mtlTexSrc = static_cast<MetalTexture*>(source.get());
        auto mtlTexDst = static_cast<MetalTexture*>(destination.get());

        [_mtlEnc copyFromTexture:mtlTexSrc->GetHandle() 
                     sourceSlice:(NSUInteger)srcBaseArrayLayer 
                     sourceLevel:(NSUInteger)srcMipLevel 
                    sourceOrigin:(MTLOrigin)MTLOriginMake(srcOrigin.x, srcOrigin.y, srcOrigin.z) 
                      sourceSize:(MTLSize)MTLSizeMake(copySize.width, copySize.height, copySize.depth) 
                       toTexture:mtlTexDst->GetHandle()
                destinationSlice:(NSUInteger)dstBaseArrayLayer 
                destinationLevel:(NSUInteger)dstMipLevel 
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
    
        
        /// Resolves a multisampled source <see cref="Texture"/> into a non-multisampled destination <see cref="Texture"/>.
        /// <param name="source">The source of the resolve operation. Must be a multisampled <see cref="Texture"/>
        /// (<see cref="Texture.SampleCount"/> > 1).</param>
        /// <param name="destination">The destination of the resolve operation. Must be a non-multisampled <see cref="Texture"/>
        /// (<see cref="Texture.SampleCount"/> == 1).</param>
        void MetalTransferCmdEnc::ResolveTexture(const common::sp<ITexture>& source, const common::sp<ITexture>& destination) 
        {}

} // namespace alloy::mtl
