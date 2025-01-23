#include "MetalShader.h"

#include "MetalDevice.h"
#include "MtlTypeCvt.h"

#include <vector>

#include <metal_irconverter/metal_irconverter.h>

//#define IR_PRIVATE_IMPLEMENTATION // define only once in an implementation file
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

namespace alloy::mtl {

#if 0
common::sp<MetalShader> MetalShader::Make(common::sp<MetalDevice>&& dev, const alloy::IShader::Description& desc, const std::string& src){
    
    auto _dev = dev->GetHandle();
    
    @autoreleasepool {
        
        MTLCompileOptions* options = [MTLCompileOptions new];
        auto nsSrc = [NSString stringWithUTF8String:src.c_str()];
        
        NSError* err = nullptr;
        
        auto shaderLib = [_dev newLibraryWithSource:nsSrc options:options error:&err];
        
        if(err != nullptr) {
            return nullptr;
        }
        
        auto fnName = [NSString stringWithUTF8String:desc.entryPoint.c_str()];
        
        auto shaderFn = [shaderLib newFunctionWithName:fnName];
        
        auto shaderObj = new MetalShader(std::move(dev), desc);
        shaderObj->_shaderLib = shaderLib;
        shaderObj->_shaderFn = shaderFn;
        
        return common::sp(shaderObj);
        
    }
    
}
#endif

common::sp<MetalShader> MetalShader::MakeFromDXIL(common::sp<MetalDevice>&& dev,
                                                  const alloy::IShader::Description& desc,
                                                  const std::span<std::uint8_t>& bin)
{
    
    auto shaderObj = new MetalShader(std::move(dev), desc);
    
    
    shaderObj->_dxil.resize(bin.size());// = bin;
    
    std::copy(bin.begin(), bin.end(), shaderObj->_dxil.begin());
    
    return common::sp(shaderObj);
}



id<MTLLibrary> NewLibraryFromAIR(id<MTLDevice> dev,
                                 IRMetalLibBinary* pMetallib)
{
    // Retrieve Metallib:
    size_t metallibSize = IRMetalLibGetBytecodeSize(pMetallib);
    auto metallib = (uint8_t*)malloc(metallibSize);
    IRMetalLibGetBytecode(pMetallib, metallib);

    // Store the metallib to custom format or disk, or use to create a MTLLibrary.
    NSError* error = nil;
    dispatch_data_t data =
        dispatch_data_create(metallib, metallibSize, nullptr, DISPATCH_DATA_DESTRUCTOR_FREE);

    auto res = [dev newLibraryWithData:data error:&error];

    if(error == nil) {
        return res;
    } else {
        [error release];
        return nil;
    }

}

id<MTLLibrary> TranspileDXILShader(
    id<MTLDevice> device,
    IShader::Stage stage,
    const IRRootSignature* rootSig,
    const std::string& entryPoint,
    const std::span<uint8_t>& dxil
){
    bool success = false;
    id<MTLLibrary> sh = nil;
    IRCompiler* pCompiler = IRCompilerCreate();
    IRCompilerSetEntryPointName(pCompiler,entryPoint.c_str());
    IRCompilerSetMinimumDeploymentTarget(pCompiler,IROperatingSystem_macOS,"14.0.0");
    IRCompilerSetGlobalRootSignature(pCompiler, rootSig);

    IRObject* pDXIL = IRObjectCreateFromDXIL(
                                             dxil.data(),
                                             dxil.size(),
        IRBytecodeOwnershipNone);

    // Compile DXIL to Metal IR:
    IRError* pError = nullptr;
    IRObject* pOutIR = IRCompilerAllocCompileAndLink(
        pCompiler,
        NULL,
        pDXIL,
        &pError);

    if (!pOutIR)
    {
        // Inspect pError to determine cause.
        IRErrorDestroy( pError );
    } else {
        
        IRShaderStage cvtShaderStage;
        switch(stage)
        {
            case IShader::Stage::Fragment : cvtShaderStage = IRShaderStageFragment; break;
            case IShader::Stage::Compute : cvtShaderStage = IRShaderStageCompute; break;
            default: break;
        }
        // Retrieve Metallib:
        IRMetalLibBinary* pMetallib = IRMetalLibBinaryCreate();
        IRObjectGetMetalLibBinary(pOutIR, cvtShaderStage, pMetallib);
        
        sh = NewLibraryFromAIR(device, pMetallib);
        
        //delete [] metallib;
        IRMetalLibBinaryDestroy(pMetallib);
    }

    IRObjectDestroy(pDXIL);
    IRObjectDestroy(pOutIR);
    IRCompilerDestroy(pCompiler);
    return sh;
}

    MetalShader::MetalShader(
        common::sp<MetalDevice>&& dev,
        const alloy::IShader::Description& desc
    )
        : IShader(desc)
        , _mtlDev(dev)
        {}

    MetalShader::~MetalShader(){
        //[_shaderFn release];
        //[_shaderLib release];
    }


MetalVertexShaderStage TranspileVertexShader(id<MTLDevice> dev,
                                             const std::vector<VertexLayout>& vertexLayouts,
                                             const IRRootSignature* rootSig,
                                             const std::string& entryPoint,
                                             std::span<uint8_t> dxil
) {
    MetalVertexShaderStage shaderStage{};
    
    IRCompiler* pCompiler = IRCompilerCreate();
    //IRObject* pIR = // input IR
    IRCompilerSetEntryPointName(pCompiler,entryPoint.c_str());
    IRCompilerSetGlobalRootSignature(pCompiler, rootSig);
    IRCompilerSetMinimumDeploymentTarget(pCompiler,IROperatingSystem_macOS,"14.0.0");

    IRObject* pDXIL = IRObjectCreateFromDXIL(dxil.data(),
                                             dxil.size(),
                                             IRBytecodeOwnershipNone);

    // Synthesize a separate stage-in function by providing a vertex input layout:
    IRVersionedInputLayoutDescriptor inputDesc {};
    inputDesc.version = IRInputLayoutDescriptorVersion_1;
    
    auto& iaDesc = inputDesc.desc_1_0;
    
    int targetIndex = 0;
    //int targetLocation = 0;
    for (int binding = 0; binding < vertexLayouts.size(); binding++)
    {
        auto& inputDesc = vertexLayouts[binding];
        //bindingDescs[binding].binding = binding;
        //bindingDescs[binding].inputRate = (inputDesc.instanceStepRate != 0)
        //    ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE
        //    : VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
        //bindingDescs[binding].stride = inputDesc.stride;
        
        unsigned currentOffset = 0;
        for (int location = 0; location < inputDesc.elements.size(); location++)
        {
            auto& inputElement = inputDesc.elements[location];

            iaDesc.semanticNames[targetIndex]/*LPCSTR*/ = SemanticToStr(inputElement.semantic.name);
            iaDesc.inputElementDescs[targetIndex]./*UINT*/ semanticIndex = inputElement.semantic.slot;
            iaDesc.inputElementDescs[targetIndex]./*DXGI_FORMAT*/ format = AlToMtlShaderDataType(inputElement.format);
            iaDesc.inputElementDescs[targetIndex]./*UINT*/ inputSlot = binding;
            iaDesc.inputElementDescs[targetIndex]./*UINT*/ alignedByteOffset = inputElement.offset != 0
                                                            ? inputElement.offset
                                                            : currentOffset;
            //TODO: [DXC, Vk] Support instanced draw?
            iaDesc.inputElementDescs[targetIndex]./*D3D12_INPUT_CLASSIFICATION*/ inputSlotClass = IRInputClassificationPerVertexData;
            iaDesc.inputElementDescs[targetIndex]./*UINT*/ instanceDataStepRate = 0;
            
            targetIndex += 1;
            currentOffset += FormatHelpers::GetSizeInBytes(inputElement.format);
        }

        //targetLocation += inputDesc.elements.size();
    }
    iaDesc.numElements = targetIndex;
    
    do {
        
        //Compile vertex shader
        IRError* pError = nullptr;
        IRCompilerSetStageInGenerationMode(pCompiler,
                                           IRStageInCodeGenerationModeUseSeparateStageInFunction);
        IRObject* pIR = IRCompilerAllocCompileAndLink(pCompiler, nullptr, pDXIL, &pError);
        
        // Validate pIR != null and no error.
        if(pError) {
            //Print error
            
            IRErrorDestroy(pError);
            break;
        }
        
        
        IRMetalLibBinary* pVertexStageMetalLib = IRMetalLibBinaryCreate();
        bool success = IRObjectGetMetalLibBinary(pIR, IRShaderStageVertex, pVertexStageMetalLib);
        
        shaderStage.vertexLib = NewLibraryFromAIR(dev, pVertexStageMetalLib);
        
        
        //Generate stage in shader
        IRShaderReflection* pVertexReflection = IRShaderReflectionCreate();
        IRObjectGetReflection(pIR, IRShaderStageVertex, pVertexReflection);
        
        
        IRMetalLibBinary* pStageInMetalLib = IRMetalLibBinaryCreate();
        success = IRMetalLibSynthesizeStageInFunction(pCompiler,
                                                           pVertexReflection,
                                                           &inputDesc,
                                                           pStageInMetalLib);
        
        IRShaderReflectionDestroy(pVertexReflection);
        IRMetalLibBinaryDestroy(pVertexStageMetalLib);
        IRObjectDestroy(pIR);
        
        // Verify success
        
        // Verify success
        
        if (pError)
        {
            IRErrorDestroy(pError);
        }
        
        shaderStage.stageInLib = NewLibraryFromAIR(dev, pStageInMetalLib);
        
        IRMetalLibBinaryDestroy(pStageInMetalLib);
    }while(0);
    
    IRCompilerDestroy(pCompiler);
    
    return shaderStage;
}

}
