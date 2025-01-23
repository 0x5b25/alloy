#include "MetalPipeline.h"

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <cassert>

#include "MetalDevice.h"
#include "alloy/Shader.hpp"
#include "alloy/Types.hpp"
#include "alloy/FixedFunctions.hpp"
#include "MtlTypeCvt.h"
#include "MetalShader.h"
#include "MetalBindableResource.h"

namespace alloy::mtl {

MTLStencilOperation _AlToMtlStencilOp(const DepthStencilStateDescription::StencilBehavior::Operation& from ) {
    using AlStencilOp = DepthStencilStateDescription::StencilBehavior::Operation;
    switch (from) {
    case AlStencilOp::Keep: return MTLStencilOperationKeep;
    case AlStencilOp::Zero: return MTLStencilOperationZero;
    case AlStencilOp::Replace: return MTLStencilOperationReplace;
    case AlStencilOp::Invert: return MTLStencilOperationInvert;
    case AlStencilOp::IncrementAndClamp: return MTLStencilOperationIncrementClamp;
    case AlStencilOp::IncrementAndWrap: return MTLStencilOperationIncrementWrap;
    case AlStencilOp::DecrementAndClamp: return MTLStencilOperationDecrementClamp;
    case AlStencilOp::DecrementAndWrap: return MTLStencilOperationDecrementWrap;
    }
}

MTLCompareFunction _AlToMtlCompareOp(const ComparisonKind& from) {
    switch (from) {
    
    case ComparisonKind::Never: return MTLCompareFunctionNever;
    case ComparisonKind::Always: return MTLCompareFunctionAlways;
    case ComparisonKind::Less: return MTLCompareFunctionLess;
    case ComparisonKind::LessEqual: return MTLCompareFunctionLessEqual;
    case ComparisonKind::Equal: return MTLCompareFunctionEqual;
    case ComparisonKind::GreaterEqual: return MTLCompareFunctionGreaterEqual;
    case ComparisonKind::Greater: return MTLCompareFunctionGreater;
    case ComparisonKind::NotEqual: return MTLCompareFunctionNotEqual;
    }
}

MTLBlendOperation _AlToMtlBlendOp(const alloy::AttachmentStateDescription::BlendFunction& from){
    switch (from) {
    
    case AttachmentStateDescription::BlendFunction::Add: return MTLBlendOperationAdd;
    case AttachmentStateDescription::BlendFunction::Subtract: return MTLBlendOperationSubtract;
    case AttachmentStateDescription::BlendFunction::ReverseSubtract: return MTLBlendOperationReverseSubtract;
    case AttachmentStateDescription::BlendFunction::Minimum: return MTLBlendOperationMin;
    case AttachmentStateDescription::BlendFunction::Maximum: return MTLBlendOperationMax;
    }
}

MTLBlendFactor _AlToMtlBlendFactor(const AttachmentStateDescription::BlendFactor& from, bool isAlpha) {
    switch (from) {

    case AttachmentStateDescription::BlendFactor::Zero: return MTLBlendFactorZero;
    case AttachmentStateDescription::BlendFactor::One: return MTLBlendFactorOne;
    case AttachmentStateDescription::BlendFactor::SourceAlpha: return MTLBlendFactorSourceAlpha;
    case AttachmentStateDescription::BlendFactor::InverseSourceAlpha: return MTLBlendFactorOneMinusSourceAlpha;
    case AttachmentStateDescription::BlendFactor::DestinationAlpha: return MTLBlendFactorDestinationAlpha;
    case AttachmentStateDescription::BlendFactor::InverseDestinationAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
    case AttachmentStateDescription::BlendFactor::SourceColor: return MTLBlendFactorSourceColor;
    case AttachmentStateDescription::BlendFactor::InverseSourceColor: return MTLBlendFactorOneMinusSourceColor;
    case AttachmentStateDescription::BlendFactor::DestinationColor: return MTLBlendFactorDestinationColor;
    case AttachmentStateDescription::BlendFactor::InverseDestinationColor: return MTLBlendFactorOneMinusDestinationColor;
    case AttachmentStateDescription::BlendFactor::BlendFactor: return isAlpha? MTLBlendFactorBlendAlpha
                                                                        : MTLBlendFactorBlendColor;
    case AttachmentStateDescription::BlendFactor::InverseBlendFactor: return isAlpha? MTLBlendFactorOneMinusBlendAlpha
                                                                               : MTLBlendFactorOneMinusBlendColor;

    }
}


common::sp<MetalGfxPipeline> MetalGfxPipeline::Make(
    common::sp<MetalDevice>&& dev, 
    const alloy::GraphicsPipelineDescription& desc
){
    auto mtlDev = dev->GetHandle();
    auto alPipelineObj = new MetalGfxPipeline(std::move(dev));
    @autoreleasepool{
    bool success = true;
    //Create MTLRenderPipelineState
    if(success) {
        MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor new] autorelease];

        // Rasterizer State
        
        switch (desc.rasterizerState.cullMode) {
            case RasterizerStateDescription::FaceCullMode::Back: alPipelineObj->_rasterizerState.cullMode = MTLCullModeBack; break;
            case RasterizerStateDescription::FaceCullMode::Front: alPipelineObj->_rasterizerState.cullMode = MTLCullModeFront; break;
            case RasterizerStateDescription::FaceCullMode::None:alPipelineObj->_rasterizerState.cullMode = MTLCullModeNone; break;
        }

        switch (desc.rasterizerState.frontFace) {
            case RasterizerStateDescription::FrontFace::Clockwise:
                alPipelineObj->_rasterizerState.frontFace = MTLWindingClockwise; break;
            case RasterizerStateDescription::FrontFace::CounterClockwise:
                alPipelineObj->_rasterizerState.frontFace = MTLWindingCounterClockwise; break;
        }

        switch (desc.rasterizerState.fillMode) {

        case RasterizerStateDescription::PolygonFillMode::Solid:
            alPipelineObj->_rasterizerState.fillMode = MTLTriangleFillModeFill; break;
        case RasterizerStateDescription::PolygonFillMode::Wireframe:
            alPipelineObj->_rasterizerState.fillMode = MTLTriangleFillModeLines; break;
        }
            //rsCI.lineWidth = 1.f;

        //Shader stages
        ///#TODO: Engage metal shader converter
        /*
        for(auto& sh : desc.shaderSet.shaders) {

            auto shader = static_cast<ShaderImpl*>(sh.get());

            switch (shader->GetDesc().stage) {
            
                case IShader::Stage::Vertex:
                    pipelineDesc.vertexFunction = shader->GetFuncHandle();
                break;

                case IShader::Stage::Fragment:
                    pipelineDesc.fragmentFunction = shader->GetFuncHandle();
                break;

                case IShader::Stage::Geometry:
                case IShader::Stage::TessellationControl:
                case IShader::Stage::TessellationEvaluation:
                case IShader::Stage::Compute: 
                default:
                    //Unsupported shader stages
                    assert(false);
                break;
            }
        }
         
        alPipelineObj->_shaders = desc.shaderSet.shaders;
        */
        auto mtlResLayout = common::PtrCast<MetalResourceLayout>(desc.resourceLayout.get());
        {
            auto shader = desc.shaderSet.fragmentShader;
            auto& shaderDesc = shader->GetDesc();
            auto fragLib = TranspileDXILShader(mtlDev,
                                               IShader::Stage::Fragment,
                                               mtlResLayout->GetHandle(),
                                               shaderDesc.entryPoint, shader->GetByteCode());
            [fragLib autorelease];
            pipelineDesc.fragmentFunction = [fragLib newFunctionWithName:fragLib.functionNames.firstObject];
        }
        
        {
            auto shader = desc.shaderSet.vertexShader;
            auto& shaderDesc = shader->GetDesc();
            auto vertLib = TranspileVertexShader(mtlDev,
                                                 desc.shaderSet.vertexLayouts,
                                                 mtlResLayout->GetHandle(),
                                                 shaderDesc.entryPoint, shader->GetByteCode());
            
            [vertLib.vertexLib autorelease];
            [vertLib.stageInLib autorelease];
            
            pipelineDesc.vertexFunction =
            [vertLib.vertexLib newFunctionWithName:vertLib.vertexLib.functionNames.firstObject];
            
            MTLLinkedFunctions* linkedFunctions = [[MTLLinkedFunctions new] autorelease];
            linkedFunctions.functions = @[
                [vertLib.stageInLib newFunctionWithName:vertLib.stageInLib.functionNames.firstObject]
            ];
            pipelineDesc.vertexLinkedFunctions = linkedFunctions;
        }

        // ... continue configuring the pipeline state descriptor...

        // ViewportState
        // Vulkan spec specifies that there must be 1 viewport no matter
        // dynamic viewport state enabled or not...

            // Setup the output pixel format to match the pixel format of the metal kit view
            //pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
            //pipelineDescriptor

        // Pipeline Layout
        alPipelineObj->_pipelineLayout = desc.resourceLayout;

        // Multisample
        pipelineDesc.rasterSampleCount = (unsigned)desc.outputs.sampleCount;

        // Vertex Input State
        //For metal version until metal 3, we only support max 31 attributes per descriptor
#if 0
        unsigned attributeIdx = 0;
        for(unsigned slotIdx = 0; slotIdx < desc.shaderSet.vertexLayouts.size(); slotIdx++) {

            auto& slotDesc = desc.shaderSet.vertexLayouts[slotIdx]; 
            for(auto& element : slotDesc.elements) {

                pipelineDesc.vertexDescriptor.attributes[attributeIdx].format = AlToMtlShaderDataType(element.format);
                pipelineDesc.vertexDescriptor.attributes[attributeIdx].offset = element.offset;
                pipelineDesc.vertexDescriptor.attributes[attributeIdx].bufferIndex = slotIdx;
                attributeIdx++;

            }

            switch (slotDesc.instanceStepRate) {
            case 0: 
                pipelineDesc.vertexDescriptor.layouts[slotIdx].stepFunction= MTLVertexStepFunctionPerVertex;
                pipelineDesc.vertexDescriptor.layouts[slotIdx].stepRate = 1;
            break;

            default:
                pipelineDesc.vertexDescriptor.layouts[slotIdx].stepFunction= MTLVertexStepFunctionPerInstance;
                pipelineDesc.vertexDescriptor.layouts[slotIdx].stepRate = slotDesc.instanceStepRate;
            break;
            }

            pipelineDesc.vertexDescriptor.layouts[slotIdx].stride = slotDesc.stride;
        }
#endif
        alPipelineObj->_vertexLayouts = desc.shaderSet.vertexLayouts;
        // Attachment States
        pipelineDesc.alphaToCoverageEnabled = desc.attachmentState.alphaToCoverageEnabled;
        for(unsigned i = 0; i < desc.attachmentState.colorAttachments.size(); i++) {
            auto& attachment = desc.attachmentState.colorAttachments[i];
            pipelineDesc.colorAttachments[i].pixelFormat = alloy::mtl::AlToMtlPixelFormat(attachment.format);

            if(attachment.colorWriteMask.r) pipelineDesc.colorAttachments[i].writeMask |= MTLColorWriteMaskRed;
            if(attachment.colorWriteMask.g) pipelineDesc.colorAttachments[i].writeMask |= MTLColorWriteMaskGreen;
            if(attachment.colorWriteMask.b) pipelineDesc.colorAttachments[i].writeMask |= MTLColorWriteMaskBlue;
            if(attachment.colorWriteMask.a) pipelineDesc.colorAttachments[i].writeMask |= MTLColorWriteMaskAlpha;


            pipelineDesc.colorAttachments[i].blendingEnabled = attachment.blendEnabled;
            pipelineDesc.colorAttachments[i].rgbBlendOperation = _AlToMtlBlendOp(attachment.colorFunction);
            pipelineDesc.colorAttachments[i].alphaBlendOperation = _AlToMtlBlendOp(attachment.alphaFunction);

            pipelineDesc.colorAttachments[i].sourceRGBBlendFactor 
                = _AlToMtlBlendFactor(attachment.sourceColorFactor, false);
            pipelineDesc.colorAttachments[i].sourceAlphaBlendFactor
                = _AlToMtlBlendFactor(attachment.sourceAlphaFactor, true);

            pipelineDesc.colorAttachments[i].destinationRGBBlendFactor
                = _AlToMtlBlendFactor(attachment.destinationColorFactor, false);
            pipelineDesc.colorAttachments[i].destinationAlphaBlendFactor
                = _AlToMtlBlendFactor(attachment.destinationAlphaFactor, true);
        }

        if(desc.attachmentState.depthStencilAttachment.has_value()) {
            pipelineDesc.depthAttachmentPixelFormat
                = alloy::mtl::AlToMtlPixelFormat(desc.attachmentState.depthStencilAttachment.value().depthStencilFormat);
            pipelineDesc.stencilAttachmentPixelFormat 
                = alloy::mtl::AlToMtlPixelFormat(desc.attachmentState.depthStencilAttachment.value().depthStencilFormat);
        }

        alPipelineObj->_blendColor = desc.attachmentState.blendConstant;

        // Input Assembly
        switch (desc.primitiveTopology) {

        case PrimitiveTopology::TriangleList:
            pipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
            alPipelineObj->_mtlPrimitiveTopology = MTLPrimitiveTypeTriangle; break;
        case PrimitiveTopology::TriangleStrip:
            pipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
            alPipelineObj->_mtlPrimitiveTopology = MTLPrimitiveTypeTriangleStrip; break;
        case PrimitiveTopology::LineList: 
            pipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine;
            alPipelineObj->_mtlPrimitiveTopology = MTLPrimitiveTypeLine; break;
        case PrimitiveTopology::LineStrip:
            pipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine;
            alPipelineObj->_mtlPrimitiveTopology = MTLPrimitiveTypeLineStrip; break;
        case PrimitiveTopology::PointList: 
            pipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;
            alPipelineObj->_mtlPrimitiveTopology = MTLPrimitiveTypePoint; break;
            break;
        }

        NSError* error = nullptr;
        auto pipelineState = [mtlDev newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];

        if(error != nullptr) {
            success = false;
            [error release];
        } else {
            alPipelineObj->_mtlPipelineState = pipelineState;
        }
    }

    // Depth Stencil State
    if(success) {
        MTLDepthStencilDescriptor* depthStencilDescriptor = [[MTLDepthStencilDescriptor new] autorelease];
        if(desc.depthStencilState.depthTestEnabled) {
            depthStencilDescriptor.depthCompareFunction = _AlToMtlCompareOp(desc.depthStencilState.depthComparison);
        } else {
            depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
        }

        depthStencilDescriptor.depthWriteEnabled = depthStencilDescriptor.depthWriteEnabled;

        if(desc.depthStencilState.stencilTestEnabled) {
            auto& alDssDesc = desc.depthStencilState;

            MTLStencilDescriptor* frontStencilDesc = [MTLStencilDescriptor new];
            depthStencilDescriptor.frontFaceStencil = frontStencilDesc;

            MTLStencilDescriptor* backStencilDesc = [MTLStencilDescriptor new];
            depthStencilDescriptor.backFaceStencil = backStencilDesc;

            frontStencilDesc.stencilFailureOperation = _AlToMtlStencilOp(alDssDesc.stencilFront.fail);
            frontStencilDesc.depthStencilPassOperation = _AlToMtlStencilOp(alDssDesc.stencilFront.pass);
            frontStencilDesc.depthFailureOperation = _AlToMtlStencilOp(alDssDesc.stencilFront.depthFail);
            frontStencilDesc.stencilCompareFunction = _AlToMtlCompareOp(alDssDesc.stencilFront.comparison);
            frontStencilDesc.readMask = alDssDesc.stencilReadMask;
            frontStencilDesc.writeMask = alDssDesc.stencilWriteMask;
            //frontStencilDesc.reference = vdDssDesc.stencilReference;

            backStencilDesc.stencilFailureOperation = _AlToMtlStencilOp(alDssDesc.stencilBack.fail);
            backStencilDesc.depthStencilPassOperation = _AlToMtlStencilOp(alDssDesc.stencilBack.pass);
            backStencilDesc.depthFailureOperation = _AlToMtlStencilOp(alDssDesc.stencilBack.depthFail);
            backStencilDesc.stencilCompareFunction = _AlToMtlCompareOp(alDssDesc.stencilBack.comparison);
            backStencilDesc.readMask = alDssDesc.stencilReadMask;
            backStencilDesc.writeMask = alDssDesc.stencilWriteMask;
            //backStencilDesc.reference = vdDssDesc.stencilReference;
        }
        
        alPipelineObj->_mtlDepthStencilState = [mtlDev newDepthStencilStateWithDescriptor:depthStencilDescriptor];
    }

    if(!success) {
        delete alPipelineObj;
        return nullptr;
    }
        
    return common::sp(alPipelineObj);

    }
}

MetalGfxPipeline::~MetalGfxPipeline(){

}


    MetalComputePipeline:: ~MetalComputePipeline() {

    }

    common::sp<MetalComputePipeline> MetalComputePipeline::Make(
        common::sp<MetalDevice>&& dev, 
        const alloy::ComputePipelineDescription& desc
    ) {
        return {};
    }

    void MetalGfxPipeline::BindToCmdBuf(id<MTLRenderCommandEncoder> enc) {
        @autoreleasepool {
            // Bind pipeline state
            [enc setRenderPipelineState:_mtlPipelineState];

            // Bind primitive topology

            // Bind rasterizer state
            [enc setFrontFacingWinding:_rasterizerState.frontFace];
            [enc setCullMode:_rasterizerState.cullMode];
            [enc setTriangleFillMode:_rasterizerState.fillMode];

            // Bind blend color
            [enc setBlendColorRed:_blendColor.r
                            green:_blendColor.g
                             blue:_blendColor.b
                            alpha:_blendColor.a];

            // Bind depth stencil state
            [enc setDepthStencilState:_mtlDepthStencilState];
        }

      // Bind resources
    }
} // namespace alloy::mtl

