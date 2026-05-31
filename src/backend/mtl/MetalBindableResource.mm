#include "MetalBindableResource.h"

#include "MetalDevice.h"
#include "MetalTexture.h"
#include "MtlTypeCvt.h"

#include <cstring>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include <stdexcept>
#include <cassert>

namespace alloy::mtl {

    namespace {
        constexpr uint32_t kIRRootSignatureFlagCBVSRVUAVHeapDirectlyIndexed = 0x400;
        constexpr uint32_t kIRRootSignatureFlagSamplerHeapDirectlyIndexed = 0x800;

        IRRootSignatureFlags T2RootSignatureFlags() {
            return (IRRootSignatureFlags)(
                IRRootSignatureFlagAllowInputAssemblerInputLayout
                | IRRootSignatureFlagAllowStreamOutput
                | kIRRootSignatureFlagCBVSRVUAVHeapDirectlyIndexed
                | kIRRootSignatureFlagSamplerHeapDirectlyIndexed);
        }
    }

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
        _T2BindlessCtorTag,
        const common::sp<MetalDevice>& dev,
        const IResourceLayout::Description& desc
    )
        : IResourceLayout(desc)
        , _dev(dev)
        , _rootSig {}
    {
        assert(dev->GetAdapter().GetAdapterInfo().resourceBindingModel
               == ResourceBindingModel::DescriptorHeap);
        assert(desc.shaderResources.empty());

        std::vector<IRRootParameter1> rootParams;
        _rootSigSizeInBytes = 0;
        _descHeapCount = 0;

        uint32_t rootConstantRequested = 0;
        for(auto& rootConsts : desc.pushConstants) {
            auto& rootParam = rootParams.emplace_back();
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
        rsDesc.desc_1_1.Flags = T2RootSignatureFlags();

        IRError* pRootSigError = nullptr;
        _rootSig = IRRootSignatureCreateFromDescriptor(&rsDesc, &pRootSigError);
        assert(_rootSig != nullptr);
        return;
    }


    MetalResourceLayout::MetalResourceLayout(
        _FixedSizeCtorTag,
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

        bool hasCombinedDescriptorTable = false;
        for(auto& elem : desc.shaderResources) {
            hasCombinedDescriptorTable |=
                elem.kind != IBindableResource::ResourceKind::Sampler;
        }

        const uint32_t samplerHeapIndex = hasCombinedDescriptorTable ? 1 : 0;
        uint32_t combinedDescriptorOffset = 0;
        uint32_t samplerDescriptorOffset = 0;
        uint32_t linearResourceOffset = 0;
        std::vector<MetalResourceLayout::SlotLocation> slotLocations(
            desc.shaderResources.size());

        for(unsigned i = 0; i < desc.shaderResources.size(); i++) {

            auto& elem = desc.shaderResources[i];
            IRDescriptorRange1* pDescRange = nullptr;
            const bool isSampler =
                elem.kind == IBindableResource::ResourceKind::Sampler;

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
            pDescRange->NumDescriptors = elem.bindingCount;
            pDescRange->OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend;// this appends the range to the end of the root signature descriptor tables

            auto& location = slotLocations[i];
            location.valid = true;
            location.linearResourceOffset = linearResourceOffset;
            if(isSampler) {
                location.heapIndex = samplerHeapIndex;
                location.startingDescriptorIndex = samplerDescriptorOffset;
                samplerDescriptorOffset += elem.bindingCount;
            } else {
                location.heapIndex = 0;
                location.startingDescriptorIndex = combinedDescriptorOffset;
                combinedDescriptorOffset += elem.bindingCount;
            }
            linearResourceOffset += elem.bindingCount;
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
        _slotLocations = std::move(slotLocations);
    }


    MetalResourceLayout::~MetalResourceLayout() {

        IRRootSignatureDestroy( _rootSig );
    }


    namespace {
        uint32_t GetRequiredBoundResourceCount(
            const IResourceLayout::Description& layoutDesc
        ) {
            uint32_t count = 0;
            for(auto& slotDesc : layoutDesc.shaderResources) {
                count += slotDesc.bindingCount;
            }
            return count;
        }

        void CountArgumentTableEntries(
            const IResourceLayout::Description& layoutDesc,
            uint32_t& combinedDescriptorCount,
            uint32_t& samplerDescriptorCount
        ) {
            combinedDescriptorCount = 0;
            samplerDescriptorCount = 0;

            for(auto& slotDesc : layoutDesc.shaderResources) {
                if(slotDesc.kind == IBindableResource::ResourceKind::Sampler) {
                    samplerDescriptorCount += slotDesc.bindingCount;
                } else {
                    combinedDescriptorCount += slotDesc.bindingCount;
                }
            }
        }

        std::vector<IMutableResourceSet::WriteBinding> MakeWritesFromBoundResources(
            const IResourceSet::Description& desc
        ) {
            std::vector<IMutableResourceSet::WriteBinding> writes;
            if(desc.boundResources.empty()) {
                return writes;
            }

            auto& layoutDesc = desc.layout->GetDesc();
            writes.reserve(layoutDesc.shaderResources.size());

            uint32_t resIdx = 0;
            for(uint32_t layoutSlot = 0; layoutSlot < layoutDesc.shaderResources.size(); ++layoutSlot) {
                auto& slotDesc = layoutDesc.shaderResources[layoutSlot];

                auto& write = writes.emplace_back();
                write.layoutSlot = layoutSlot;
                write.firstArrayElement = 0;
                write.resources.reserve(slotDesc.bindingCount);
                for(uint32_t i = 0; i < slotDesc.bindingCount; ++i) {
                    write.resources.push_back(desc.boundResources[resIdx++]);
                }
            }

            return writes;
        }

        IRDescriptorTableEntry* OffsetDescriptorTableEntry(
            id<MTLBuffer> argBuf,
            uint64_t heapBaseOffset,
            uint32_t descriptorIndex
        ) {
            auto entryStartCpuAddr = (std::uintptr_t)[argBuf contents];
            return (IRDescriptorTableEntry*)(
                entryStartCpuAddr
                + heapBaseOffset
                + descriptorIndex * sizeof(IRDescriptorTableEntry));
        }
    }

    MetalResourceSetBase::MetalResourceSetBase(
        const common::sp<MetalDevice>& dev,
        const common::sp<MetalResourceLayout>& layout
    )
        : _dev(dev)
        , _layout(layout)
        , _argBuf(nil)
        , _shaderResHeapGPUVA(0)
        , _samplerHeapGPUVA(0)
        , _combinedResHeapSize(0)
        , _samplerHeapSize(0)
        , _boundResources(GetRequiredBoundResourceCount(_layout->GetDesc()))
    {
    }

    MetalResourceSetBase::~MetalResourceSetBase() {
        @autoreleasepool {
            [_argBuf release];
        }
    }

    void MetalResourceSetBase::AllocateArgumentBuffer() {
        auto& layoutDesc = _layout->GetDesc();

        std::uint32_t combinedResCnt = 0, samplerCnt = 0;
        CountArgumentTableEntries(layoutDesc, combinedResCnt, samplerCnt);

        _combinedResHeapSize = combinedResCnt * sizeof(IRDescriptorTableEntry);
        _samplerHeapSize = samplerCnt * sizeof(IRDescriptorTableEntry);

        uint64_t argBufferSize = _combinedResHeapSize + _samplerHeapSize;

        @autoreleasepool {
            if(argBufferSize == 0) {
                return;
            }

            MTLResourceOptions mtlDesc{};

            mtlDesc |= MTLResourceStorageModeShared;

            _argBuf = [_dev->GetHandle() newBufferWithLength:argBufferSize options:mtlDesc];
            [_argBuf setLabel:@"_ResHeaps"];

            auto entryStartCpuAddr = [_argBuf contents];
            std::memset(entryStartCpuAddr, 0, argBufferSize);

            auto baseAddr = [_argBuf gpuAddress];
            auto heapAddr = (uint64_t)baseAddr;

            if(_combinedResHeapSize > 0) {
                _shaderResHeapGPUVA = heapAddr;
            }

            if(_samplerHeapSize > 0) {
                _samplerHeapGPUVA = heapAddr + _combinedResHeapSize;
            }
        }
    }

    void MetalResourceSetBase::UpdateInternal(
        const std::span<const IMutableResourceSet::WriteBinding>& writes
    ) {
        const auto& slotDescs = _layout->GetDesc().shaderResources;

        assert(_boundResources.size() == GetRequiredBoundResourceCount(_layout->GetDesc()));

        for(auto& write : writes) {
            auto& slotDesc = slotDescs[write.layoutSlot];
            if(write.resources.empty()) {
                continue;
            }

            auto location = _layout->GetSlotLocation(write.layoutSlot);
            const uint64_t heapBaseOffset =
                slotDesc.kind == IBindableResource::ResourceKind::Sampler
                    ? _combinedResHeapSize
                    : 0;

            auto descriptor = OffsetDescriptorTableEntry(
                _argBuf,
                heapBaseOffset,
                location.startingDescriptorIndex + write.firstArrayElement);

            for(uint32_t i = 0; i < write.resources.size(); ++i) {
                auto* dst = descriptor + i;
                auto& elem = write.resources[i];

                if(elem != nullptr) {
                    switch (slotDesc.kind)
                    {
                    case IBindableResource::ResourceKind::UniformBuffer :
                    case IBindableResource::ResourceKind::StorageBuffer : {
                        auto* range = common::PtrCast<BufferRange>(elem.get());
                        auto* rangedMtlBuffer =
                            PtrCast<MetalBuffer>(range->GetBufferObject().get());

                        auto baseGPUAddr = [rangedMtlBuffer->GetHandle() gpuAddress];
                        auto byteCnt = range->GetShape().GetSizeInBytes();

                        // DX12/MetalShaderConverter CBVs require 256-byte size alignment.
                        assert(slotDesc.kind != IBindableResource::ResourceKind::UniformBuffer
                               || byteCnt % 256 == 0);

                        IRDescriptorTableSetBuffer(
                            dst,
                            baseGPUAddr + range->GetShape().GetOffsetInBytes(),
                            0);
                    }break;

                    case IBindableResource::ResourceKind::Texture : {
                        auto* texView = common::PtrCast<ITextureView>(elem.get());
                        auto* mtlTex =
                            common::PtrCast<MetalTexture>(texView->GetTextureObject().get());

                        IRDescriptorTableSetTexture(dst, mtlTex->GetHandle(), 0, 0);
                    }break;

                    case IBindableResource::ResourceKind::Sampler : {
                        auto* sampler = common::PtrCast<MetalSampler>(elem.get());
                        IRDescriptorTableSetSampler(dst, sampler->GetHandle(), 0);
                    }break;
                    }
                } else {
                    std::memset(dst, 0, sizeof(IRDescriptorTableEntry));
                }

                _boundResources[location.linearResourceOffset
                                + write.firstArrayElement
                                + i] = elem;
            }
        }
    }

    std::vector<id<MTLResource>> MetalResourceSetBase::GetUseResources() const {

        std::vector<id<MTLResource>> useRes {};
        if(_argBuf) {
            useRes.push_back(_argBuf);
        }

        auto& layoutDesc = _layout->GetDesc();

        for(unsigned i = 0, resIdx = 0; i < layoutDesc.shaderResources.size(); i++) {

            auto& elemDesc = layoutDesc.shaderResources[i];
            for(uint32_t arrIdx = 0; arrIdx < elemDesc.bindingCount; ++arrIdx) {
                auto& elem = _boundResources[resIdx++];
                if(elem == nullptr) {
                    continue;
                }

                switch (elemDesc.kind)
                {
                case IBindableResource::ResourceKind::UniformBuffer :
                case IBindableResource::ResourceKind::StorageBuffer : {

                    const auto* range = common::PtrCast<BufferRange>(elem.get());
                    const auto* rangedMtlBuffer = PtrCast<MetalBuffer>(range->GetBufferObject().get());
                    useRes.push_back(rangedMtlBuffer->GetHandle());
                }break;
                case IBindableResource::ResourceKind::Texture : {
                    const auto* texView = common::PtrCast<ITextureView>(elem.get());
                    const auto* mtlTex = common::PtrCast<MetalTexture>(texView->GetTextureObject().get());
                    useRes.push_back(mtlTex->GetHandle());
                }break;

                case IBindableResource::ResourceKind::Sampler : {

                    //auto* sampler = PtrCast<MetalSampler>(elem.get());
                    //useRes.push_back(sampler->GetHandle());
                }break;
                }
            }
        }
        return useRes;
    }

    MetalResourceSet::MetalResourceSet(
        const common::sp<MetalDevice>& dev,
        const IResourceSet::Description& desc
    )
        : IResourceSet()
        , MetalResourceSetBase(dev, common::SPCast<MetalResourceLayout>(desc.layout))
    {
    }

    MetalResourceSet::~MetalResourceSet() {
    }

    common::sp<MetalResourceSet> MetalResourceSet::Make(
        const common::sp<MetalDevice>& dev,
        const IResourceSet::Description& desc
    ) {
        auto requiredBoundResourceCount =
            GetRequiredBoundResourceCount(desc.layout->GetDesc());

        auto resourceSet = common::sp(new MetalResourceSet(dev, desc));
        resourceSet->AllocateArgumentBuffer();

        if(!desc.boundResources.empty()) {
            auto writes = MakeWritesFromBoundResources(desc);
            resourceSet->UpdateInternal(writes);
        }

        return resourceSet;
    }

    MetalMutableResourceSet::MetalMutableResourceSet(
        const common::sp<MetalDevice>& dev,
        const IMutableResourceSet::Description& desc
    )
        : IMutableResourceSet()
        , MetalResourceSetBase(dev, common::SPCast<MetalResourceLayout>(desc.layout))
    {
    }

    MetalMutableResourceSet::~MetalMutableResourceSet() {
    }

    common::sp<IMutableResourceSet> MetalMutableResourceSet::Make(
        const common::sp<MetalDevice>& dev,
        const IMutableResourceSet::Description& desc
    ) {
        auto resourceSet = common::sp(new MetalMutableResourceSet(dev, desc));
        resourceSet->AllocateArgumentBuffer();

        if(!desc.initialWrites.empty()) {
            resourceSet->UpdateInternal(desc.initialWrites);
        }

        return resourceSet;
    }

    void MetalMutableResourceSet::Update(
        const std::span<const IMutableResourceSet::WriteBinding>& writes
    ) {
        UpdateInternal(writes);
    }
}
