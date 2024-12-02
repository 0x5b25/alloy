#include "DXCBindableResource.hpp"

#include "veldrid/common/Common.hpp"
#include "D3DTypeCvt.hpp"
#include "DXCDevice.hpp"
#include "DXCTexture.hpp"
#include "directx/d3d12.h"

namespace Veldrid
{ 

    sp<ResourceLayout> DXCResourceLayout::Make(const sp<DXCDevice> &dev, const Description &desc)
    {
        auto pDev = dev->GetDevice();

        Shader::Stages combinedShaderResAccess;
        Shader::Stages samplerAccess;

        std::vector<D3D12_DESCRIPTOR_RANGE> combinedShaderResDescTableRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> samplerDescTableRanges;

        for(unsigned i = 0; i < desc.elements.size(); i++) {
            
            auto& elem = desc.elements[i];
            D3D12_DESCRIPTOR_RANGE* pDescRange = nullptr;

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
            pDescRange->RegisterSpace = 0; // space 0. can usually be zero
            pDescRange->NumDescriptors = 1; // we only have one constant buffer, so the range is only 1
            pDescRange->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables
        
        }

        std::vector<D3D12_ROOT_PARAMETER> rootParams;

        if(!combinedShaderResDescTableRanges.empty()) {
            rootParams.emplace_back(); auto& rootParam = rootParams.back();
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParam.DescriptorTable.NumDescriptorRanges = combinedShaderResDescTableRanges.size();
            rootParam.DescriptorTable.pDescriptorRanges = combinedShaderResDescTableRanges.data();
            rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        if(!samplerDescTableRanges.empty()) {
            rootParams.emplace_back(); auto& rootParam = rootParams.back();
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParam.DescriptorTable.NumDescriptorRanges = samplerDescTableRanges.size();
            rootParam.DescriptorTable.pDescriptorRanges = samplerDescTableRanges.data();
            rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
        rootSigDesc.NumParameters = rootParams.size();
        rootSigDesc.pParameters = rootParams.data();
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                          | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        auto hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
        if (FAILED(hr))
        {
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
        hr = pDev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        if (FAILED(hr))
        {
            return nullptr;
        }

        auto layout = sp(new DXCResourceLayout(dev, desc));
        layout->_rootSig = rootSignature;

        return layout;
    }



    sp<ResourceSet> DXCResourceSet::Make(
        const sp<DXCDevice>& dev,
        const Description& desc
    ) {
        auto pDev = dev->GetDevice();

        auto& layoutDesc = desc.layout->GetDesc();

        //Root signature can only have 64 DWORDS


        Shader::Stages combinedShaderResAccess;
        Shader::Stages samplerAccess;

        //std::vector<D3D12_DESCRIPTOR_RANGE> combinedShaderResDescTableRanges;
        //std::vector<D3D12_DESCRIPTOR_RANGE> samplerDescTableRanges;

        std::vector<IBindableResource*> combinedBindables;
        std::vector<IBindableResource*> samplerBindables;

        for(unsigned i = 0; i < layoutDesc.elements.size(); i++) {
            
            auto& elemDesc = layoutDesc.elements[i];
            auto& elem = desc.boundResources[i];
            
            switch (elemDesc.kind)
            {
            case IBindableResource::ResourceKind::UniformBuffer :
            case IBindableResource::ResourceKind::StorageBuffer :
            case IBindableResource::ResourceKind::Texture : 
                combinedBindables.push_back(elem.get());
                break;
            case IBindableResource::ResourceKind::Sampler : 
                samplerBindables.push_back(elem.get());
                break;
            }
        }

        std::vector<ID3D12DescriptorHeap*> descHeap;
        //Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap;
        //std::vector<D3D12_ROOT_PARAMETER> rootParams;

        if(!combinedBindables.empty()) {
            descHeap.emplace_back();
            auto& combinedShaderResHeap = descHeap.back();

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = combinedBindables.size();
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            auto hr = pDev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&combinedShaderResHeap));
            if (FAILED(hr))
            {
                //Running = false;
            }
        }

        if(!samplerBindables.empty()) {
            descHeap.emplace_back();
            auto& samplerHeap = descHeap.back();

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = samplerBindables.size();
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            auto hr = pDev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&samplerHeap));
            if (FAILED(hr))
            {
                //Running = false;
            }
        }

        
        uint64_t combinedShaderResCnt = 0;
        uint64_t samplerCnt = 0;

        auto combinedIncrSize = pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto samplerIncrSize = pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        for(unsigned i = 0; i < layoutDesc.elements.size(); i++) {
            
            auto& elemDesc = layoutDesc.elements[i];
            auto& elem = desc.boundResources[i];

            switch (elemDesc.kind)
            {
            case IBindableResource::ResourceKind::UniformBuffer : {
                auto combinedDescHeap = descHeap.front();

                auto* range = PtrCast<BufferRange>(elem.get());
                auto* rangedDXCBuffer = static_cast<const DXCBuffer*>(range->GetBufferObject());

                auto baseGPUAddr = rangedDXCBuffer->GetHandle()->GetGPUVirtualAddress();

                D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
                desc.BufferLocation = baseGPUAddr + range->GetShape().offsetInElements;
                desc.SizeInBytes = range->GetShape().elementCount;

                auto heapSlot = combinedDescHeap->GetCPUDescriptorHandleForHeapStart();
                heapSlot.ptr += combinedIncrSize * combinedShaderResCnt;

                pDev->CreateConstantBufferView(&desc, heapSlot);

                combinedShaderResCnt++;
            }break;
            case IBindableResource::ResourceKind::StorageBuffer :
            case IBindableResource::ResourceKind::Texture : {

                //{
                //DXGI_FORMAT Format;
                //D3D12_UAV_DIMENSION ViewDimension;
                //union 
                //    {
                //    D3D12_BUFFER_UAV Buffer;
                //    D3D12_TEX1D_UAV Texture1D;
                //    D3D12_TEX1D_ARRAY_UAV Texture1DArray;
                //    D3D12_TEX2D_UAV Texture2D;
                //    D3D12_TEX2D_ARRAY_UAV Texture2DArray;
                //    D3D12_TEX2DMS_UAV Texture2DMS;
                //    D3D12_TEX2DMS_ARRAY_UAV Texture2DMSArray;
                //    D3D12_TEX3D_UAV Texture3D;
                //    } 	;
                //}
                
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
                ID3D12Resource* pRes;

                if(elemDesc.kind == IBindableResource::ResourceKind::StorageBuffer) {
                    
                    auto* range = PtrCast<BufferRange>(elem.get());
                    auto* rangedDXCBuffer = static_cast<const DXCBuffer*>(range->GetBufferObject());
                    auto& shape = range->GetShape();

                    uavDesc.Buffer.FirstElement = shape.offsetInElements;
                    uavDesc.Buffer.NumElements = shape.elementCount;
                    uavDesc.Buffer.StructureByteStride = shape.elementSizeInBytes;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

                    pRes = rangedDXCBuffer->GetHandle();
                } else {
                    auto* texView = PtrCast<TextureView>(elem.get());
                    auto* dxcTex = PtrCast<DXCTexture>(texView->GetTarget().get());
                    auto& texDesc = dxcTex->GetDesc();
                    auto& viewDesc = texView->GetDesc();

                    switch(texDesc.type) {
                        case Texture::Description::Type::Texture1D : {
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

                        case Texture::Description::Type::Texture2D : {
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

                        case Texture::Description::Type::Texture3D : {
                            if(texDesc.arrayLayers > 1) {
                                //#TODO: handle tex array & texture type mismatch
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

                auto combinedDescHeap = descHeap.front();
                auto heapSlot = combinedDescHeap->GetCPUDescriptorHandleForHeapStart();
                heapSlot.ptr += combinedIncrSize * combinedShaderResCnt;
                pDev->CreateUnorderedAccessView(pRes, nullptr, &uavDesc, heapSlot);

                combinedShaderResCnt++;
            }break;

            case IBindableResource::ResourceKind::Sampler : {

                auto* sampler = PtrCast<Sampler>(elem.get());
                auto& desc = sampler->GetDesc();

                D3D12_SAMPLER_DESC samplerDesc {};

                samplerDesc.Filter = VdToD3DFilter(desc.filter, desc.comparisonKind != nullptr);
                samplerDesc.AddressU = VdToD3DSamplerAddressMode(desc.addressModeU);
                samplerDesc.AddressV = VdToD3DSamplerAddressMode(desc.addressModeV);
                samplerDesc.AddressW = VdToD3DSamplerAddressMode(desc.addressModeW);
                samplerDesc.ComparisonFunc = VdToD3DCompareOp(*desc.comparisonKind);

                switch (desc.borderColor) {
                    case Sampler::Description::BorderColor::TransparentBlack : 
                       samplerDesc.BorderColor[0] = 0;
                       samplerDesc.BorderColor[1] = 0;
                       samplerDesc.BorderColor[2] = 0;
                       samplerDesc.BorderColor[3] = 0; break;
                       
                    case Sampler::Description::BorderColor::OpaqueBlack : 
                       samplerDesc.BorderColor[0] = 0;
                       samplerDesc.BorderColor[1] = 0;
                       samplerDesc.BorderColor[2] = 0;
                       samplerDesc.BorderColor[3] = 1; break;
                       
                    case Sampler::Description::BorderColor::OpaqueWhite : 
                       samplerDesc.BorderColor[0] = 1;
                       samplerDesc.BorderColor[1] = 1;
                       samplerDesc.BorderColor[2] = 1;
                       samplerDesc.BorderColor[3] = 1; break;
                }

                samplerDesc.MipLODBias = desc.lodBias;
                samplerDesc.MaxAnisotropy = desc.maximumAnisotropy;
                samplerDesc.MinLOD = desc.minimumLod;
                samplerDesc.MaxLOD = desc.maximumLod;

                
                auto samplerDescHeap = descHeap.back();
                auto heapSlot = samplerDescHeap->GetCPUDescriptorHandleForHeapStart();
                heapSlot.ptr += samplerIncrSize * samplerCnt;
                pDev->CreateSampler(&samplerDesc, heapSlot);

                samplerCnt++;
            }break;
            }

        }

        auto resSet = sp(new DXCResourceSet(dev, desc));
        resSet->_descHeap = std::move(descHeap);

        return resSet;
    }

    
    DXCResourceSet::~DXCResourceSet() {
        for(auto* pHeap : _descHeap) {
            pHeap->Release();
        }
    }

} // namespace Veldrid


