#include "DXCBindableResource.hpp"

#include "alloy/common/Common.hpp"
#include "D3DTypeCvt.hpp"
#include "DXCDevice.hpp"
#include "DXCTexture.hpp"
#include "DXCContext.hpp"

#include <stdexcept>
#include <d3d12.h>

namespace alloy::dxc
{ 
    namespace {

        ID3D12RootSignature* CreateT2RootSignature(
            DXCDevice* dev,
            const std::vector<D3D12_ROOT_PARAMETER1>& rootParams
        ) {
            auto& d3d12Dll = dev->GetContext().GetD3D12Dll();
            constexpr auto flags 
                = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT
                | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
                | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
                ;

            ID3DBlob* signature;
            HRESULT hr = E_FAIL;

            std::vector<D3D12_ROOT_PARAMETER> rootParamsV1(rootParams.size());
            for(std::size_t i = 0; i < rootParams.size(); ++i) {
                auto& dst = rootParamsV1[i];
                const auto& src = rootParams[i];
                assert(src.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS);
                dst.ParameterType = src.ParameterType;
                dst.Constants = src.Constants;
                dst.ShaderVisibility = src.ShaderVisibility;
            }

            D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
            rootSigDesc.NumParameters = static_cast<UINT>(rootParamsV1.size());
            rootSigDesc.pParameters = rootParamsV1.data();
            rootSigDesc.NumStaticSamplers = 0;
            rootSigDesc.pStaticSamplers = nullptr;
            rootSigDesc.Flags = flags;

            hr = d3d12Dll.pfnD3D12SerializeRootSignature(
                &rootSigDesc,
                D3D_ROOT_SIGNATURE_VERSION_1,
                &signature,
                nullptr);
            
            ID3D12RootSignature* rootSignature;

            if(SUCCEEDED(hr)) {

                hr = dev->GetDevice()->CreateRootSignature(
                    0,
                    signature->GetBufferPointer(),
                    signature->GetBufferSize(),
                    IID_PPV_ARGS(&rootSignature));
            }

            signature->Release();

            if(FAILED(hr)) {
                return nullptr;
            }

            return rootSignature;
        }

    } // namespace

    DXCResourceLayout::~DXCResourceLayout() {
        _rootSig->Release();
    }
    
    common::sp<IResourceLayout> DXCResourceLayout::Make(
        const common::sp<DXCDevice> &dev,
        const Description &desc
    ) {
        if(desc.useGlobalHeaps) {
            return _MakeT2Bindless(dev, desc);
        } else {
            return _MakeFixedSize(dev, desc);
        }
    }
 
    common::sp<IResourceLayout> DXCResourceLayout::_MakeT2Bindless(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    ) {
        assert(dev->GetDevCaps().SupportBindless());
        assert(desc.shaderResources.empty());

        std::vector<D3D12_ROOT_PARAMETER1> rootParams;
        uint32_t rootConstantRequested = 0;
        for(auto& rootConsts : desc.pushConstants) {
            rootParams.emplace_back();
            auto& rootParam = rootParams.back();
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            rootParam.Constants.Num32BitValues = rootConsts.sizeInDwords;
            rootParam.Constants.ShaderRegister = rootConsts.bindingSlot;
            rootParam.Constants.RegisterSpace = rootConsts.bindingSpace;
            rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            rootConstantRequested += rootConsts.sizeInDwords;
        }

        assert(rootConstantRequested <= MAX_ROOT_SIGNATURE_SIZE_DW);


        auto layout = common::sp(new DXCResourceLayout(dev, desc));
        layout->_rootSig = CreateT2RootSignature(dev.get(), rootParams);
        
        if(!layout->_rootSig) {
            return nullptr;
        }

        layout->_rootConstantCount = rootConstantRequested;
        layout->_heapCount = 0;
        layout->_slotLocations.clear();
        return layout;
    }

    common::sp<IResourceLayout> DXCResourceLayout::_MakeFixedSize(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    ) {
        auto pDev = dev->GetDevice();
        auto& d3d12Dll = dev->GetContext().GetD3D12Dll();

        IShader::Stages combinedShaderResAccess;
        IShader::Stages samplerAccess;

        std::vector<D3D12_DESCRIPTOR_RANGE> combinedShaderResDescTableRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> samplerDescTableRanges;

        bool hasCombinedDescriptorTable = false;
        for(auto& elem : desc.shaderResources) {
            hasCombinedDescriptorTable |=
                elem.kind != IBindableResource::ResourceKind::Sampler;
        }

        const uint32_t samplerHeapIndex = hasCombinedDescriptorTable ? 1 : 0;
        uint32_t combinedDescriptorOffset = 0;
        uint32_t samplerDescriptorOffset = 0;
        uint32_t linearResourceOffset = 0;
        std::vector<DXCResourceLayout::SlotLocation> slotLocations(desc.shaderResources.size());

        for(unsigned i = 0; i < desc.shaderResources.size(); i++) {
            
            auto& elem = desc.shaderResources[i];
            D3D12_DESCRIPTOR_RANGE* pDescRange = nullptr;
            const bool isSampler = elem.kind == IBindableResource::ResourceKind::Sampler;

            switch (elem.kind)
            {
            case IBindableResource::ResourceKind::UniformBuffer : {
                combinedShaderResDescTableRanges.emplace_back();
                pDescRange = &combinedShaderResDescTableRanges.back();
                pDescRange->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                combinedShaderResAccess |= elem.stages;
            }break;
            case IBindableResource::ResourceKind::StorageBuffer :
            case IBindableResource::ResourceKind::Texture : {
                combinedShaderResDescTableRanges.emplace_back();
                pDescRange = &combinedShaderResDescTableRanges.back();

                if(elem.options.writable)
                    pDescRange->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                else
                    pDescRange->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

                combinedShaderResAccess |= elem.stages;
            }break;
            case IBindableResource::ResourceKind::Sampler : {
                samplerDescTableRanges.emplace_back();
                pDescRange = &samplerDescTableRanges.back();
                pDescRange->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                samplerAccess |= elem.stages;
            }break;
            }

            pDescRange->BaseShaderRegister = elem.bindingSlot; // start index of the shader registers in the range
            pDescRange->RegisterSpace = elem.bindingSpace; // space 0. can usually be zero
            pDescRange->NumDescriptors = elem.bindingCount; // Support for array binding
            pDescRange->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

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

        uint32_t rootConstantCnt = MAX_ROOT_SIGNATURE_SIZE_DW;
        if(!combinedShaderResDescTableRanges.empty())
            rootConstantCnt -= 1;
        if(!samplerDescTableRanges.empty())
            rootConstantCnt -= 1;

        

        std::vector<D3D12_ROOT_PARAMETER> rootParams;

        
        std::uint32_t heapCount = 0;

        if(!combinedShaderResDescTableRanges.empty()) {
            heapCount++;
            rootParams.emplace_back(); auto& rootParam = rootParams.back();
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParam.DescriptorTable.NumDescriptorRanges = combinedShaderResDescTableRanges.size();
            rootParam.DescriptorTable.pDescriptorRanges = combinedShaderResDescTableRanges.data();
            rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        if(!samplerDescTableRanges.empty()) {
            heapCount++;
            rootParams.emplace_back(); auto& rootParam = rootParams.back();
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParam.DescriptorTable.NumDescriptorRanges = samplerDescTableRanges.size();
            rootParam.DescriptorTable.pDescriptorRanges = samplerDescTableRanges.data();
            rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        uint32_t rootConstantRequested = 0;
        for(auto& rootConsts : desc.pushConstants) {
            rootParams.emplace_back(); auto& rootParam = rootParams.back();
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            rootParam.Constants.Num32BitValues = rootConsts.sizeInDwords;
            rootParam.Constants.ShaderRegister = rootConsts.bindingSlot;
            rootParam.Constants.RegisterSpace = rootConsts.bindingSpace;
            rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            rootConstantRequested += rootConsts.sizeInDwords;
        }

        assert(rootConstantCnt >= rootConstantRequested &&
            "Too many root constants, root signature exceeded 256 DWORDS");

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
        rootSigDesc.NumParameters = rootParams.size();
        rootSigDesc.pParameters = rootParams.data();
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                          | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

        ID3DBlob* signature;
        ID3D12RootSignature* rootSignature;
        auto hr = d3d12Dll.pfnD3D12SerializeRootSignature(
            &rootSigDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            nullptr);

        if (SUCCEEDED(hr))
        {
            hr = pDev->CreateRootSignature(
                0, 
                signature->GetBufferPointer(),
                signature->GetBufferSize(),
                IID_PPV_ARGS(&rootSignature));
        }

        signature->Release();

        if (FAILED(hr))
        {
            return nullptr;
        }

        auto layout = common::sp(new DXCResourceLayout(dev, desc));
        layout->_rootSig = rootSignature;
        layout->_rootConstantCount = rootConstantRequested;
        layout->_heapCount = heapCount;
        layout->_slotLocations = std::move(slotLocations);

        return layout;
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

        void CountDescriptorHeaps(
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

        bool RequiresPopulatedDescriptor(
            D3D12_RESOURCE_BINDING_TIER tier,
            IBindableResource::ResourceKind kind
        ) {
            if(tier == D3D12_RESOURCE_BINDING_TIER_1) {
                return true;
            }

            if(tier == D3D12_RESOURCE_BINDING_TIER_2) {
                return kind != IBindableResource::ResourceKind::Sampler;
            }

            return false;
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
            for(uint32_t layoutSlot = 0;
                layoutSlot < layoutDesc.shaderResources.size();
                ++layoutSlot)
            {
                auto& slotDesc = layoutDesc.shaderResources[layoutSlot];
                assert(resIdx + slotDesc.bindingCount <= desc.boundResources.size() &&
                        "DX12 ResourceSet boundResources count is smaller than ResourceLayout capacity.");

                auto& write = writes.emplace_back();
                write.layoutSlot = layoutSlot;
                write.firstArrayElement = 0;
                write.resources.reserve(slotDesc.bindingCount);
                for(uint32_t i = 0; i < slotDesc.bindingCount; ++i) {
                    write.resources.push_back(desc.boundResources[resIdx++]);
                }
            }

            assert(resIdx == desc.boundResources.size() &&
                    "DX12 ResourceSet boundResources contains more entries than ResourceLayout requires.");

            return writes;
        }

        ID3D12DescriptorHeap* CreateShaderVisibleDescriptorHeap(
            ID3D12Device* dev,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            uint32_t descriptorCount
        ) {
            if(descriptorCount == 0) {
                return nullptr;
            }

            ID3D12DescriptorHeap* heap = nullptr;
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = descriptorCount;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heapDesc.Type = type;
            auto hr = dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
            if(FAILED(hr)) {
                throw std::runtime_error("DX12 failed to create ResourceSet descriptor heap.");
            }

            return heap;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE OffsetDescriptor(
            D3D12_CPU_DESCRIPTOR_HANDLE handle,
            uint32_t incrementSize,
            uint32_t descriptorOffset
        ) {
            handle.ptr += static_cast<SIZE_T>(incrementSize) * descriptorOffset;
            return handle;
        }

        void WriteNullDXCDescriptor(
            ID3D12Device* pDev,
            const IResourceLayout::ShaderResourceDescription& elemDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE heapSlot
        ) {
            switch (elemDesc.kind)
            {
            case IBindableResource::ResourceKind::UniformBuffer:
                pDev->CreateConstantBufferView(nullptr, heapSlot);
                break;

            case IBindableResource::ResourceKind::StorageBuffer: {
                if(elemDesc.options.writable) {
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
                    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
                    uavDesc.Buffer.FirstElement = 0;
                    uavDesc.Buffer.NumElements = 0;
                    uavDesc.Buffer.StructureByteStride = 0;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                    pDev->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, heapSlot);
                } else {
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                    srvDesc.Buffer.FirstElement = 0;
                    srvDesc.Buffer.NumElements = 0;
                    srvDesc.Buffer.StructureByteStride = 0;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    pDev->CreateShaderResourceView(nullptr, &srvDesc, heapSlot);
                }
            } break;

            case IBindableResource::ResourceKind::Texture: {
                // Layouts do not currently carry texture dimension/format, so use a harmless
                // 2D descriptor for unaccessed null texture slots.
                if(elemDesc.options.writable) {
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
                    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    pDev->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, heapSlot);
                } else {
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    srvDesc.Texture2D.MipLevels = 1;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    pDev->CreateShaderResourceView(nullptr, &srvDesc, heapSlot);
                }
            } break;

            case IBindableResource::ResourceKind::Sampler: {
                D3D12_SAMPLER_DESC samplerDesc {};
                samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                samplerDesc.MinLOD = 0.0f;
                samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
                samplerDesc.MaxAnisotropy = 1;
                samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
                pDev->CreateSampler(&samplerDesc, heapSlot);
            } break;
            }
        }
    }
    
    DXCResourceSetBase::DXCResourceSetBase(
        const common::sp<DXCDevice>& dev,
        const common::sp<DXCResourceLayout>& layout
    )
        : dev(dev)
        , _layout(layout)
        , _boundResources(GetRequiredBoundResourceCount(_layout->GetDesc()))
    {

    }

    DXCResourceSet::DXCResourceSet(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    ) 
        : IResourceSet()
        , DXCResourceSetBase(dev, common::SPCast<DXCResourceLayout>(desc.layout))
    {}


    DXCResourceSetBase::~DXCResourceSetBase() {
        for(auto* heap : _descHeap) {
            heap->Release();
        }
    }

    void DXCResourceSetBase::AllocateDescriptorHeaps() {
        auto pDev = dev->GetDevice();
        const auto& layoutDesc = _layout->GetDesc();

        uint32_t combinedDescriptorCount = 0;
        uint32_t samplerDescriptorCount = 0;
        CountDescriptorHeaps(
            layoutDesc,
            combinedDescriptorCount,
            samplerDescriptorCount);

        if(combinedDescriptorCount > 0) {
            _descHeap.push_back(CreateShaderVisibleDescriptorHeap(
                pDev,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                combinedDescriptorCount));
        }

        if(samplerDescriptorCount > 0) {
            _descHeap.push_back(CreateShaderVisibleDescriptorHeap(
                pDev,
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                samplerDescriptorCount));
        }

        auto tier = dev->GetDevCaps().options.ResourceBindingTier;
        for(uint32_t layoutSlot = 0;
            layoutSlot < layoutDesc.shaderResources.size();
            ++layoutSlot)
        {
            auto& slotDesc = layoutDesc.shaderResources[layoutSlot];
            if(!RequiresPopulatedDescriptor(tier, slotDesc.kind)) {
                continue;
            }

            auto location = _layout->GetSlotLocation(layoutSlot);
            if(!location.valid || location.heapIndex >= _descHeap.size()) {
                continue;
            }

            const auto heapType =
                slotDesc.kind == IBindableResource::ResourceKind::Sampler
                    ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
                    : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            const auto incrementSize =
                pDev->GetDescriptorHandleIncrementSize(heapType);

            auto descriptor = _descHeap[location.heapIndex]->GetCPUDescriptorHandleForHeapStart();
            descriptor = OffsetDescriptor(
                descriptor,
                incrementSize,
                location.startingDescriptorIndex);

            for(uint32_t i = 0; i < slotDesc.bindingCount; ++i) {
                auto dst = OffsetDescriptor(descriptor, incrementSize, i);
                WriteNullDXCDescriptor(pDev, slotDesc, dst);
            }
        }
    }

    void DXCResourceSetBase::UpdateInternal(
        const std::span<const IMutableResourceSet::WriteBinding>& writes
    ) {
        auto pDev = dev->GetDevice();
        const auto& slotDescs = _layout->GetDesc().shaderResources;

        assert(_boundResources.size() == GetRequiredBoundResourceCount(_layout->GetDesc()));

        const auto d3dHwTier = dev->GetDevCaps().options.ResourceBindingTier;

        for(auto& write : writes) {

            auto& slotDesc = slotDescs[write.layoutSlot];
            if(write.resources.empty()) {
                continue;
            }

            auto location = _layout->GetSlotLocation(write.layoutSlot);

            const auto heapType =
                slotDesc.kind == IBindableResource::ResourceKind::Sampler
                    ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
                    : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            const auto incrementSize =
                pDev->GetDescriptorHandleIncrementSize(heapType);

            // Get handle corresponding to firstArrayElement in write op
            auto descriptor = _descHeap[location.heapIndex]->GetCPUDescriptorHandleForHeapStart();
            descriptor = OffsetDescriptor(
                descriptor,
                incrementSize,
                location.startingDescriptorIndex + write.firstArrayElement);

            for(uint32_t i = 0; i < write.resources.size(); ++i) {
                auto dst = OffsetDescriptor(descriptor, incrementSize, i);
                if(write.resources[i] != nullptr) {
                    auto& elem = write.resources[i];
                    switch (slotDesc.kind)
                    {
                    case IBindableResource::ResourceKind::UniformBuffer : {
                        const auto* range = PtrCast<BufferRange>(elem.get());
                        const auto* rangedDXCBuffer =
                            PtrCast<DXCBuffer>(range->GetBufferObject().get());

                        auto baseGPUAddr = rangedDXCBuffer->GetHandle()->GetGPUVirtualAddress();
                        auto byteCnt = range->GetShape().GetSizeInBytes();

                        // DX12 requires CBVs to be 256-byte aligned.
                        assert(byteCnt % 256 == 0);

                        D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
                        desc.BufferLocation = baseGPUAddr + range->GetShape().offsetInElements;
                        desc.SizeInBytes = byteCnt;

                        pDev->CreateConstantBufferView(&desc, dst);
                    }break;

                    case IBindableResource::ResourceKind::StorageBuffer :
                    case IBindableResource::ResourceKind::Texture : {
                        if(slotDesc.options.writable) {
                            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
                            ID3D12Resource* pRes;

                            if(slotDesc.kind == IBindableResource::ResourceKind::StorageBuffer) {
                                auto* range = PtrCast<BufferRange>(elem.get());
                                auto* rangedDXCBuffer =
                                    PtrCast<DXCBuffer>(range->GetBufferObject().get());
                                auto& shape = range->GetShape();

                                uavDesc.Format = DXGI_FORMAT_UNKNOWN;
                                uavDesc.Buffer.FirstElement = shape.offsetInElements;
                                uavDesc.Buffer.NumElements = shape.elementCount;
                                uavDesc.Buffer.StructureByteStride = shape.elementSizeInBytes;
                                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

                                pRes = rangedDXCBuffer->GetHandle();
                            } else {
                                const auto* texView = PtrCast<ITextureView>(elem.get());
                                const auto* dxcTex =
                                    PtrCast<DXCTexture>(texView->GetTextureObject().get());
                                auto& texDesc = dxcTex->GetDesc();
                                auto& viewDesc = texView->GetDesc();
                                uavDesc.Format = VdToD3DPixelFormat(
                                    texDesc.format,
                                    texDesc.usage.depthStencil);

                                switch(texDesc.type) {
                                    case ITexture::Description::Type::Texture1D : {
                                        if(texDesc.arrayLayers > 1) {
                                            uavDesc.Texture1DArray.ArraySize = viewDesc.arrayLayers;
                                            uavDesc.Texture1DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                            uavDesc.Texture1DArray.MipSlice = viewDesc.baseMipLevel;
                                            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                                        } else {
                                            uavDesc.Texture1D.MipSlice = viewDesc.baseMipLevel;
                                            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                                        }
                                    }break;

                                    case ITexture::Description::Type::Texture2D : {
                                        if(texDesc.arrayLayers > 1) {
                                            uavDesc.Texture2DArray.ArraySize = viewDesc.arrayLayers;
                                            uavDesc.Texture2DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                            uavDesc.Texture2DArray.PlaneSlice = 0;
                                            uavDesc.Texture2DArray.MipSlice = viewDesc.baseMipLevel;
                                            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                                        } else {
                                            uavDesc.Texture2D.PlaneSlice = 0;
                                            uavDesc.Texture2D.MipSlice = viewDesc.baseMipLevel;
                                            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                                        }
                                    }break;

                                    case ITexture::Description::Type::Texture3D : {
                                        if(texDesc.arrayLayers > 1) {
                                            assert(false);
                                        } else {
                                            uavDesc.Texture3D.FirstWSlice = 0;
                                            uavDesc.Texture3D.WSize = -1;
                                            uavDesc.Texture3D.MipSlice = viewDesc.baseMipLevel;
                                            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                                        }
                                    }break;
                                }

                                pRes = dxcTex->GetHandle();
                            }

                            pDev->CreateUnorderedAccessView(pRes, nullptr, &uavDesc, dst);
                        } else {
                            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
                            srvDesc.Shader4ComponentMapping =
                                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                            ID3D12Resource* pRes;

                            if(slotDesc.kind == IBindableResource::ResourceKind::StorageBuffer) {
                                const auto* range = PtrCast<BufferRange>(elem.get());
                                const auto* rangedDXCBuffer =
                                    PtrCast<DXCBuffer>(range->GetBufferObject().get());
                                auto& shape = range->GetShape();

                                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                                srvDesc.Buffer.FirstElement = shape.offsetInElements;
                                srvDesc.Buffer.NumElements = shape.elementCount;
                                srvDesc.Buffer.StructureByteStride = shape.elementSizeInBytes;
                                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

                                pRes = rangedDXCBuffer->GetHandle();
                            } else {
                                auto* texView = PtrCast<ITextureView>(elem.get());
                                auto* dxcTex =
                                    PtrCast<DXCTexture>(texView->GetTextureObject().get());
                                auto& texDesc = dxcTex->GetDesc();
                                auto& viewDesc = texView->GetDesc();

                                srvDesc.Format = VdToD3DPixelFormat(
                                    texDesc.format,
                                    texDesc.usage.depthStencil);

                                switch(texDesc.type) {
                                    case ITexture::Description::Type::Texture1D : {
                                        if(texDesc.arrayLayers > 1) {
                                            srvDesc.Texture1DArray.ArraySize = viewDesc.arrayLayers;
                                            srvDesc.Texture1DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                            srvDesc.Texture1DArray.MipLevels = viewDesc.mipLevels;
                                            srvDesc.Texture1DArray.MostDetailedMip = viewDesc.baseMipLevel;
                                            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                                        } else {
                                            srvDesc.Texture1D.MostDetailedMip = viewDesc.baseMipLevel;
                                            srvDesc.Texture1D.MipLevels = viewDesc.mipLevels;
                                            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                                        }
                                    }break;

                                    case ITexture::Description::Type::Texture2D : {
                                        if(texDesc.arrayLayers > 1) {
                                            srvDesc.Texture2DArray.ArraySize = viewDesc.arrayLayers;
                                            srvDesc.Texture2DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                            srvDesc.Texture2DArray.PlaneSlice = 0;
                                            srvDesc.Texture2DArray.MipLevels = viewDesc.mipLevels;
                                            srvDesc.Texture2DArray.MostDetailedMip = viewDesc.baseMipLevel;
                                            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                                        } else {
                                            srvDesc.Texture2D.PlaneSlice = 0;
                                            srvDesc.Texture2D.MipLevels = viewDesc.mipLevels;
                                            srvDesc.Texture2D.MostDetailedMip = viewDesc.baseMipLevel;
                                            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                                        }
                                    }break;

                                    case ITexture::Description::Type::Texture3D : {
                                        if(texDesc.arrayLayers > 1) {
                                            assert(false);
                                        } else {
                                            srvDesc.Texture3D.MostDetailedMip = viewDesc.baseMipLevel;
                                            srvDesc.Texture3D.MipLevels = viewDesc.mipLevels;
                                            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                                        }
                                    }break;
                                }

                                pRes = dxcTex->GetHandle();
                            }

                            pDev->CreateShaderResourceView(pRes, &srvDesc, dst);
                        }
                    }break;

                    case IBindableResource::ResourceKind::Sampler : {
                        auto* sampler = PtrCast<ISampler>(elem.get());
                        auto& desc = sampler->GetDesc();

                        D3D12_SAMPLER_DESC samplerDesc {};

                        samplerDesc.Filter =
                            VdToD3DFilter(desc.filter, desc.comparisonKind != nullptr);
                        samplerDesc.AddressU = VdToD3DSamplerAddressMode(desc.addressModeU);
                        samplerDesc.AddressV = VdToD3DSamplerAddressMode(desc.addressModeV);
                        samplerDesc.AddressW = VdToD3DSamplerAddressMode(desc.addressModeW);
                        if(desc.comparisonKind)
                            samplerDesc.ComparisonFunc = VdToD3DCompareOp(*desc.comparisonKind);

                        switch (desc.borderColor) {
                            case ISampler::Description::BorderColor::TransparentBlack :
                               samplerDesc.BorderColor[0] = 0;
                               samplerDesc.BorderColor[1] = 0;
                               samplerDesc.BorderColor[2] = 0;
                               samplerDesc.BorderColor[3] = 0; break;

                            case ISampler::Description::BorderColor::OpaqueBlack :
                               samplerDesc.BorderColor[0] = 0;
                               samplerDesc.BorderColor[1] = 0;
                               samplerDesc.BorderColor[2] = 0;
                               samplerDesc.BorderColor[3] = 1; break;

                            case ISampler::Description::BorderColor::OpaqueWhite :
                               samplerDesc.BorderColor[0] = 1;
                               samplerDesc.BorderColor[1] = 1;
                               samplerDesc.BorderColor[2] = 1;
                               samplerDesc.BorderColor[3] = 1; break;
                        }

                        samplerDesc.MipLODBias = desc.lodBias;
                        samplerDesc.MaxAnisotropy = desc.maximumAnisotropy;
                        samplerDesc.MinLOD = desc.minimumLod;
                        samplerDesc.MaxLOD = desc.maximumLod;

                        pDev->CreateSampler(&samplerDesc, dst);
                    }break;
                    }
                }
                else if(RequiresPopulatedDescriptor(d3dHwTier, slotDesc.kind))
                    WriteNullDXCDescriptor(pDev, slotDesc, dst);
                
                //Finally, let's update corresponding ref slots to hold
                // a stongreference
                _boundResources[location.linearResourceOffset 
                                + write.firstArrayElement
                                + i ] = write.resources[i];
            }
        }
    }

    
    IBindableResource* DXCResourceSetBase::GetBoundResource(
        uint32_t layoutSlot,
        uint32_t firstArrayElement
    ) {
        auto linearBase = _layout->GetSlotLocation(layoutSlot).linearResourceOffset;

        return _boundResources[linearBase + firstArrayElement].get();
    }


    common::sp<IResourceSet> DXCResourceSet::Make(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    ) {
        auto requiredBoundResourceCount =
            GetRequiredBoundResourceCount(desc.layout->GetDesc());

        auto resourceSet = common::sp(new DXCResourceSet(dev, desc));
        resourceSet->AllocateDescriptorHeaps();

        if(!desc.boundResources.empty()) {
            auto writes = MakeWritesFromBoundResources(desc);
            resourceSet->UpdateInternal(writes);
        }

        return common::sp<IResourceSet>(resourceSet);
    }

    
    DXCResourceSet::~DXCResourceSet() {
    }

    common::sp<IMutableResourceSet> DXCMutableResourceSet::Make(
        const common::sp<DXCDevice>& dev,
        const IMutableResourceSet::Description& desc
    ) {
        assert(dev->GetAdapter().GetAdapterInfo().resourceBindingModel
               != ResourceBindingModel::FixedBindings 
               && "DX12 mutable ResourceSet requires DescriptorIndexing support.");

        auto resSet = common::sp(new DXCMutableResourceSet(dev, desc));
        resSet->AllocateDescriptorHeaps();

        if(!desc.initialWrites.empty()) {
            resSet->UpdateInternal(desc.initialWrites);
        } else {
            auto requiredBoundResourceCount =
                GetRequiredBoundResourceCount(desc.layout->GetDesc());
            resSet->_boundResources.resize(requiredBoundResourceCount);
        }

        return resSet;
    }

    void DXCMutableResourceSet::Update(
        const std::span<const IMutableResourceSet::WriteBinding>& writes
    ) {
        UpdateInternal(writes);
    }

    DXCMutableResourceSet::DXCMutableResourceSet(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    )
        : IMutableResourceSet()
        , DXCResourceSetBase(dev, common::SPCast<DXCResourceLayout>(desc.layout))
    { }

    DXCMutableResourceSet::~DXCMutableResourceSet() {
    
    }
} // namespace alloy
