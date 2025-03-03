#include "MetalBindableResource.h"

#include "MetalDevice.h"
#include "MetalTexture.h"
#include "MtlTypeCvt.h"

#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

namespace alloy::mtl {

#if 0
    void CreateRootSignature(){
        IRVersionedRootSignatureDescriptor desc;
        desc.version = IRRootSignatureVersion_1_1;
        desc.desc_1_1.Flags = IRRootSignatureFlagNone;

        // Samplers are placed in their own table:
        desc.desc_1_1.NumStaticSamplers = 1;
        IRStaticSamplerDescriptor pSSDesc[] = { {
            .Filter = IRFilterMinMagMipLinear,
            .AddressU = IRTextureAddressModeWrap,
            .AddressV = IRTextureAddressModeWrap,
            .AddressW = IRTextureAddressModeWrap,
            .MipLODBias = 0,
            .MaxAnisotropy = 0,
            .ComparisonFunc = IRComparisonFunctionNever,
            .BorderColor = IRStaticBorderColorOpaqueBlack,
            .MinLOD = 0,
            .MaxLOD = std::numeric_limits<float>::max(),
            .ShaderRegister = 0,
            .RegisterSpace = 0,
            .ShaderVisibility = IRShaderVisibilityPixel
        } };
        desc.desc_1_1.pStaticSamplers = pSSDesc;

        // Parameters (1 texture):
        IRDescriptorRange1 ranges[1] = { [0] = {
            .RangeType = IRDescriptorRangeTypeSRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .Flags = IRDescriptorRangeFlagDataStatic,
            .OffsetInDescriptorsFromTableStart = 0
        }
        };
        IRRootParameter1 pParams[] = { {
            .ParameterType = IRRootParameterTypeDescriptorTable,
            .DescriptorTable = { .NumDescriptorRanges = 1, .pDescriptorRanges = ranges },
            .ShaderVisibility = IRShaderVisibilityPixel
        } };
        desc.desc_1_1.NumParameters = 1;
        desc.desc_1_1.pParameters = pParams;

        IRError* pRootSigError = nullptr;
        IRRootSignature* pRootSig = IRRootSignatureCreateFromDescriptor( &desc, &pRootSigError );
        if ( !pRootSig )
        {
            // handle and release error
        }

        // After compiling DXIL bytecode to Metal IR using this root signature,
        // it should have 2 entries:
        //
        // offset 0: a uint64_t referencing a table that contains a
        // void* resource (SRV).
        //
        // offset 8 (sizeof(uint_64)): a uint64_t referencing a table with
        // one sampler.

        // The sampler table should be encoded like so:
        // For each sampler:
        // 64-bits: GPU VA of the sampler.
        // 64-bits: 0
        // 64-bits: Sampler’s LOD Bias.

        // The SRV table should be encoded like so:
        // 64-bits: 0
        // 64-bits: Texture GPU Resource ID
        // 64-bits: 0

        // Use the companion header for help encoding resources into descriptor tables.

        IRCompiler* pCompiler = IRCompilerCreate();
        IRCompilerSetGlobalRootSignature( pCompiler, pRootSig );

        // Compile DXIL to Metal IR
        IRError* pError = nullptr;
        IRObject* pDXIL = IRObjectCreateFromDXIL(dxilFragmentBytecode,
                                                 dxilFragmentSize,
                                                 IRBytecodeOwnershipNone);
                                                 
        IRObject* pOutIR = IRCompilerAllocCompileAndLink(pCompiler,
                                                         NULL,
                                                         pDXIL,
                                                         &pError);

        // if pOutIR is null, inspect pError for causes. Release pError afterwards.

        IRMetalLibBinary* pMetallib = IRMetalLibBinaryCreate();
        IRObjectGetMetalLibBinary( pOutIR, IRShaderStageFragment, pMetallib );

        size_t metallibSize = IRMetalLibGetBytecodeSize( pMetallib );
        uint8_t* metallib = new uint8_t[ metallibSize ];
        if ( IRMetalLibGetBytecode( pMetallib, metallib ) == metallibSize )
        {
            // Store metallib for later use or directly create a MTLLibrary
        }

        delete [] metallib;

        IRMetalLibBinaryDestroy( pMetallib );

        IRObjectDestroy( pOutIR );
        IRObjectDestroy( pDXIL );

        IRRootSignatureDestroy( pRootSig );
        IRCompilerDestroy( pCompiler );
    }
#endif



    MetalResourceLayout::MetalResourceLayout(
        const common::sp<MetalDevice>& dev,
        const IResourceLayout::Description& desc
    )
        : IResourceLayout(desc)
        , _dev(dev)
        , _rootSig {}
    {
        //auto pDev = dev->GetHandle();

        IShader::Stages combinedShaderResAccess;
        IShader::Stages samplerAccess;
        
        
        std::vector<IRDescriptorRange1> combinedShaderResDescTableRanges;
        std::vector<IRDescriptorRange1> samplerDescTableRanges;
        
        for(unsigned i = 0; i < desc.shaderResources.size(); i++) {
            
            auto& elem = desc.shaderResources[i];
            IRDescriptorRange1* pDescRange = nullptr;

            switch (elem.kind)
            {
            case IBindableResource::ResourceKind::UniformBuffer : {
                pDescRange = &combinedShaderResDescTableRanges.emplace_back();
                pDescRange->RangeType = IRDescriptorRangeTypeCBV;
                combinedShaderResAccess |= elem.stages;
            }break;
            case IBindableResource::ResourceKind::StorageBuffer :
            case IBindableResource::ResourceKind::Texture : {
                pDescRange = &combinedShaderResDescTableRanges.emplace_back();

                if(elem.options.writable)
                    pDescRange->RangeType = IRDescriptorRangeTypeUAV;
                else
                    pDescRange->RangeType = IRDescriptorRangeTypeSRV;
                
                //pDescRange->Flags = IRDescriptorRangeFlagDataStatic;
                combinedShaderResAccess |= elem.stages;
            }break;
            case IBindableResource::ResourceKind::Sampler : {
                pDescRange = &samplerDescTableRanges.emplace_back();
                pDescRange->RangeType = IRDescriptorRangeTypeSampler;
                samplerAccess |= elem.stages;
            }break;
            }

            pDescRange->BaseShaderRegister = elem.bindingSlot; // start index of the shader registers in the range
            pDescRange->RegisterSpace = elem.bindingSpace; // space 0. can usually be zero
            pDescRange->NumDescriptors = 1; // we only have one constant buffer, so the range is only 1
            pDescRange->OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend;// this appends the range to the end of the root signature descriptor tables
        
        }
        
        
        std::vector<IRRootParameter1> rootParams;
        //auto rootParamSize = sizeof(IRDescriptorTableEntry);
        //SetBytes in metal command encoder is limited to 4KB max
        auto rootConstantCnt = 0x1000 / 4;
        _rootSigSizeInBytes = 0;
        _descHeapCount = 0;

        uint32_t rootConstantRequested = 0;

        //Each cost 2 DWORDs
        if(!combinedShaderResDescTableRanges.empty())
            rootConstantRequested += 2;
        if(!samplerDescTableRanges.empty())
            rootConstantRequested += 2;


        //std::vector<PushConstantInfo> pushConstants;

        if(!combinedShaderResDescTableRanges.empty()) {
            auto& rootParam = rootParams.emplace_back();
            rootParam.ParameterType = IRRootParameterTypeDescriptorTable;// D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParam.DescriptorTable.NumDescriptorRanges = combinedShaderResDescTableRanges.size();
            rootParam.DescriptorTable.pDescriptorRanges = combinedShaderResDescTableRanges.data();
            rootParam.ShaderVisibility = IRShaderVisibilityAll;

            _rootSigSizeInBytes += 8;
            _descHeapCount++;
        }

        if(!samplerDescTableRanges.empty()) {
            auto& rootParam = rootParams.emplace_back();
            rootParam.ParameterType = IRRootParameterTypeDescriptorTable;
            rootParam.DescriptorTable.NumDescriptorRanges = samplerDescTableRanges.size();
            rootParam.DescriptorTable.pDescriptorRanges = samplerDescTableRanges.data();
            rootParam.ShaderVisibility = IRShaderVisibilityAll;
            
            _rootSigSizeInBytes += 8;
            _descHeapCount++;
        }

        for(auto& rootConsts : desc.pushConstants) {
            rootParams.emplace_back(); auto& rootParam = rootParams.back();
            rootParam.ParameterType = IRRootParameterType32BitConstants;
            rootParam.Constants.Num32BitValues = rootConsts.sizeInDwords;
            rootParam.Constants.ShaderRegister = rootConsts.bindingSlot;
            rootParam.Constants.RegisterSpace = rootConsts.bindingSpace;
            rootParam.ShaderVisibility = IRShaderVisibilityAll;

            auto& pcInfo = _pushConstants.emplace_back();
            pcInfo.sizeInDwords = rootConsts.sizeInDwords;
            pcInfo.bindingSlot = rootConsts.bindingSlot;
            pcInfo.bindingSpace = rootConsts.bindingSpace;
            pcInfo.offsetInDwords = rootConstantRequested;

            rootConstantRequested += rootConsts.sizeInDwords;
            _rootSigSizeInBytes += rootConsts.sizeInDwords * 4;
        }

        
        IRVersionedRootSignatureDescriptor rsDesc {};
        
        rsDesc.version = IRRootSignatureVersion_1_1;
        rsDesc.desc_1_1.NumParameters = rootParams.size();
        rsDesc.desc_1_1.pParameters = rootParams.data();
        rsDesc.desc_1_1.Flags = (IRRootSignatureFlags)
                         (   IRRootSignatureFlagAllowInputAssemblerInputLayout
                          | IRRootSignatureFlagAllowStreamOutput);

        
        IRError* pRootSigError = nullptr;
        _rootSig = IRRootSignatureCreateFromDescriptor( &rsDesc, &pRootSigError );
        if ( !_rootSig )
        {
            // handle and release error
            throw std::runtime_error("Root signature creation failed!");
        }

    }


    MetalResourceLayout::~MetalResourceLayout() {
        
        IRRootSignatureDestroy( _rootSig );
    }


    MetalResourceSet::MetalResourceSet(const IResourceSet::Description& desc)
        : IResourceSet(desc)
        , _argBuf(nil)
    {
        auto layout = common::PtrCast<MetalResourceLayout>(desc.layout.get());
        auto dev = layout->_dev;
        auto& layoutDesc = layout->GetDesc();
        
        //Calculate total buffer sizes
        std::uint32_t combinedResCnt = 0, samplerCnt = 0;
        for(auto& elem : layoutDesc.shaderResources) {
            switch (elem.kind)
            {
            case IBindableResource::ResourceKind::UniformBuffer :
            case IBindableResource::ResourceKind::StorageBuffer :
            case IBindableResource::ResourceKind::Texture : {
                combinedResCnt++;
            }break;
            case IBindableResource::ResourceKind::Sampler : {
                samplerCnt++;
            }break;
            }
        }
        
        auto combinedResHeapSize = combinedResCnt * sizeof(IRDescriptorTableEntry);
        auto samplerHeapSize = samplerCnt * sizeof(IRDescriptorTableEntry);
        
        uint64_t argBufferSize = combinedResHeapSize + samplerHeapSize;
        
        @autoreleasepool {
            
            MTLResourceOptions mtlDesc{};
            
            mtlDesc |= MTLResourceStorageModeShared;
            //| MTLResourceCPUCacheModeDefaultCache;
            
            
            //mtlDesc |= MTLResourceHazardTrackingModeTracked;
            
            _argBuf = [dev->GetHandle() newBufferWithLength:argBufferSize options:mtlDesc];
            [_argBuf setLabel:@"_ResHeaps"];
            
            auto baseAddr = [_argBuf gpuAddress];
            
            auto heapArgAddr = (uint64_t*)[_argBuf contents];
            auto heapAddr = (uint64_t)baseAddr;
            
            if(combinedResHeapSize > 0) {
                _shaderResHeapGPUVA = heapAddr;
            } else {
                _shaderResHeapGPUVA = 0;
            }
            
            if(samplerHeapSize > 0) {
                _samplerHeapGPUVA = heapAddr + combinedResHeapSize;
            } else {
                _samplerHeapGPUVA = 0;
            }
            
            //MTL::Buffer* pTextureTable = _pScratch->newBuffer(sizeof(IRDescriptorTableEntry), MTL::ResourceStorageModeShared)->autorelease();
            //auto* pEntry               = (IRDescriptorTableEntry*)pTextureTable->contents();
            //IRDescriptorTableSetTexture(pEntry, _pTexture, 0, 0);
            //
            //MTL::Buffer* pSamplerTable = _pScratch->newBuffer(sizeof(IRDescriptorTableEntry), MTL::ResourceStorageModeShared)->autorelease();
            //pEntry                     = (IRDescriptorTableEntry*)pSamplerTable->contents();
            //IRDescriptorTableSetSampler(pEntry, _pSampler, 0);
            auto entryStartCpuAddr = ((uint64_t)[_argBuf contents]);
            auto* pCombinedResEntry = (IRDescriptorTableEntry*)(entryStartCpuAddr);
            auto* pSamplerEntry = (IRDescriptorTableEntry*)(entryStartCpuAddr + combinedResHeapSize);
            
            
            for(unsigned i = 0; i < layoutDesc.shaderResources.size(); i++) {
                
                auto& elemDesc = layoutDesc.shaderResources[i];
                auto& elem = desc.boundResources[i];
                
                switch (elemDesc.kind)
                {
                    case IBindableResource::ResourceKind::UniformBuffer :
                    case IBindableResource::ResourceKind::StorageBuffer : {
                        
                        auto* range = PtrCast<BufferRange>(elem.get());
                        auto* rangedMtlBuffer = static_cast<const MetalBuffer*>(range->GetBufferObject());
                        
                        auto baseGPUAddr = [rangedMtlBuffer->GetHandle() gpuAddress];
                        auto byteCnt = range->GetShape().elementCount;
                        
                        //DX12 requires CBVs have minimal alignment of 256Bytes
                        assert(byteCnt % 256 == 0);
                        
                        IRDescriptorTableSetBuffer(pCombinedResEntry,
                                                   baseGPUAddr + range->GetShape().offsetInElements,
                                                   0);
                        pCombinedResEntry++;
                    }break;
                    case IBindableResource::ResourceKind::Texture : {
                        auto* texView = PtrCast<ITextureView>(elem.get());
                        auto* mtlTex = PtrCast<MetalTexture>(texView->GetTextureObject().get());
                        auto& texDesc = mtlTex->GetDesc();
                        auto& viewDesc = texView->GetDesc();
                        auto format = AlToMtlPixelFormat(texDesc.format);
                        
                        IRDescriptorTableSetTexture(pCombinedResEntry, mtlTex->GetHandle(), 0, 0);
                        pCombinedResEntry++;
                    }break;
                        
                    case IBindableResource::ResourceKind::Sampler : {
                        
                        auto* sampler = PtrCast<MetalSampler>(elem.get());
                        auto& desc = sampler->GetDesc();
                        
                        IRDescriptorTableSetSampler(pSamplerEntry, sampler->GetHandle(), 0);
                        pSamplerEntry++;
                        
                    }break;
                }
                
            }
            
            
        }
    }


    
    MetalResourceSet::~MetalResourceSet() {
        @autoreleasepool {
            [_argBuf release];
        }
    }


    std::vector<id<MTLResource>> MetalResourceSet::GetUseResources() const {
        
        std::vector<id<MTLResource>> useRes {_argBuf};
        
        auto layout = common::PtrCast<MetalResourceLayout>(description.layout.get());
        auto dev = layout->_dev;
        auto& layoutDesc = layout->GetDesc();
        
        for(unsigned i = 0; i < layoutDesc.shaderResources.size(); i++) {
            
            auto& elemDesc = layoutDesc.shaderResources[i];
            auto& elem = description.boundResources[i];
            
            switch (elemDesc.kind)
            {
                case IBindableResource::ResourceKind::UniformBuffer :
                case IBindableResource::ResourceKind::StorageBuffer : {
                    
                    auto* range = PtrCast<BufferRange>(elem.get());
                    auto* rangedMtlBuffer = static_cast<const MetalBuffer*>(range->GetBufferObject());
                    useRes.push_back(rangedMtlBuffer->GetHandle());
                }break;
                case IBindableResource::ResourceKind::Texture : {
                    auto* texView = PtrCast<ITextureView>(elem.get());
                    auto* mtlTex = PtrCast<MetalTexture>(texView->GetTextureObject().get());
                    useRes.push_back(mtlTex->GetHandle());
                }break;
                    
                case IBindableResource::ResourceKind::Sampler : {
                    
                    //auto* sampler = PtrCast<MetalSampler>(elem.get());
                    //useRes.push_back(sampler->GetHandle());
                    
                }break;
            }
            
        }
        
        return useRes;
    }
}
