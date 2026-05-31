#include "DXCResourceHeap.hpp"

#include "D3DCommon.hpp"
#include "D3DTypeCvt.hpp"
#include "DXCDevice.hpp"
#include "DXCTexture.hpp"

#include "alloy/common/Common.hpp"

#include <cassert>
#include <limits>
#include <type_traits>

namespace alloy::dxc {

    namespace {

        D3D12_CPU_DESCRIPTOR_HANDLE OffsetDescriptor(
            D3D12_CPU_DESCRIPTOR_HANDLE handle,
            std::uint32_t descriptorSize,
            std::uint32_t index
        ) {
            handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
            return handle;
        }

        D3D12_SAMPLER_DESC MakeSamplerDesc(const ISampler::Description& desc) {
            D3D12_SAMPLER_DESC samplerDesc{};

            samplerDesc.Filter =
                VdToD3DFilter(desc.filter, desc.comparisonKind != nullptr);
            samplerDesc.AddressU = VdToD3DSamplerAddressMode(desc.addressModeU);
            samplerDesc.AddressV = VdToD3DSamplerAddressMode(desc.addressModeV);
            samplerDesc.AddressW = VdToD3DSamplerAddressMode(desc.addressModeW);
            if(desc.comparisonKind)
                samplerDesc.ComparisonFunc = VdToD3DCompareOp(*desc.comparisonKind);

            switch(desc.borderColor) {
                case ISampler::Description::BorderColor::TransparentBlack:
                    samplerDesc.BorderColor[0] = 0;
                    samplerDesc.BorderColor[1] = 0;
                    samplerDesc.BorderColor[2] = 0;
                    samplerDesc.BorderColor[3] = 0;
                    break;

                case ISampler::Description::BorderColor::OpaqueBlack:
                    samplerDesc.BorderColor[0] = 0;
                    samplerDesc.BorderColor[1] = 0;
                    samplerDesc.BorderColor[2] = 0;
                    samplerDesc.BorderColor[3] = 1;
                    break;

                case ISampler::Description::BorderColor::OpaqueWhite:
                    samplerDesc.BorderColor[0] = 1;
                    samplerDesc.BorderColor[1] = 1;
                    samplerDesc.BorderColor[2] = 1;
                    samplerDesc.BorderColor[3] = 1;
                    break;
            }

            samplerDesc.MipLODBias = desc.lodBias;
            samplerDesc.MaxAnisotropy = desc.maximumAnisotropy;
            samplerDesc.MinLOD = desc.minimumLod;
            samplerDesc.MaxLOD = desc.maximumLod;

            return samplerDesc;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC MakeBufferSrvDesc(const BufferRange& range) {
            const auto& shape = range.GetShape();

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Buffer.FirstElement = shape.offsetInElements;
            desc.Buffer.NumElements = shape.elementCount;
            desc.Buffer.StructureByteStride = shape.elementSizeInBytes;
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            return desc;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC MakeBufferUavDesc(const BufferRange& range) {
            const auto& shape = range.GetShape();

            D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Buffer.FirstElement = shape.offsetInElements;
            desc.Buffer.NumElements = shape.elementCount;
            desc.Buffer.StructureByteStride = shape.elementSizeInBytes;
            desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            return desc;
        }

        D3D12_CONSTANT_BUFFER_VIEW_DESC MakeCbvDesc(const BufferRange& range) {
            const auto* dxcBuffer =
                PtrCast<DXCBuffer>(range.GetBufferObject().get());
            const auto& shape = range.GetShape();
            const auto offsetInBytes = shape.GetOffsetInBytes();
            const auto sizeInBytes = shape.GetSizeInBytes();

            assert((offsetInBytes % 256) == 0);
            assert((sizeInBytes % 256) == 0);
            assert(sizeInBytes <= std::numeric_limits<UINT>::max());

            D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
            desc.BufferLocation =
                dxcBuffer->GetHandle()->GetGPUVirtualAddress() + offsetInBytes;
            desc.SizeInBytes = static_cast<UINT>(sizeInBytes);
            return desc;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC MakeTextureSrvDesc(const ITextureView& texView) {
            const auto* dxcTex =
                PtrCast<DXCTexture>(texView.GetTextureObject().get());
            const auto& texDesc = dxcTex->GetDesc();
            const auto& viewDesc = texView.GetDesc();

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Format = VdToD3DPixelFormat(texDesc.format, texDesc.usage.depthStencil);

            switch(texDesc.type) {
                case ITexture::Description::Type::Texture1D: {
                    if(texDesc.arrayLayers > 1) {
                        desc.Texture1DArray.ArraySize = viewDesc.arrayLayers;
                        desc.Texture1DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                        desc.Texture1DArray.MipLevels = viewDesc.mipLevels;
                        desc.Texture1DArray.MostDetailedMip = viewDesc.baseMipLevel;
                        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                    } else {
                        desc.Texture1D.MostDetailedMip = viewDesc.baseMipLevel;
                        desc.Texture1D.MipLevels = viewDesc.mipLevels;
                        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                    }
                } break;

                case ITexture::Description::Type::Texture2D: {
                    if(texDesc.arrayLayers > 1) {
                        desc.Texture2DArray.ArraySize = viewDesc.arrayLayers;
                        desc.Texture2DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                        desc.Texture2DArray.PlaneSlice = 0;
                        desc.Texture2DArray.MipLevels = viewDesc.mipLevels;
                        desc.Texture2DArray.MostDetailedMip = viewDesc.baseMipLevel;
                        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    } else {
                        desc.Texture2D.PlaneSlice = 0;
                        desc.Texture2D.MipLevels = viewDesc.mipLevels;
                        desc.Texture2D.MostDetailedMip = viewDesc.baseMipLevel;
                        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    }
                } break;

                case ITexture::Description::Type::Texture3D: {
                    assert(texDesc.arrayLayers <= 1);
                    desc.Texture3D.MostDetailedMip = viewDesc.baseMipLevel;
                    desc.Texture3D.MipLevels = viewDesc.mipLevels;
                    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                } break;
            }

            return desc;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC MakeTextureUavDesc(const ITextureView& texView) {
            const auto* dxcTex =
                PtrCast<DXCTexture>(texView.GetTextureObject().get());
            const auto& texDesc = dxcTex->GetDesc();
            const auto& viewDesc = texView.GetDesc();

            D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = VdToD3DPixelFormat(texDesc.format, texDesc.usage.depthStencil);

            switch(texDesc.type) {
                case ITexture::Description::Type::Texture1D: {
                    if(texDesc.arrayLayers > 1) {
                        desc.Texture1DArray.ArraySize = viewDesc.arrayLayers;
                        desc.Texture1DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                        desc.Texture1DArray.MipSlice = viewDesc.baseMipLevel;
                        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                    } else {
                        desc.Texture1D.MipSlice = viewDesc.baseMipLevel;
                        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                    }
                } break;

                case ITexture::Description::Type::Texture2D: {
                    if(texDesc.arrayLayers > 1) {
                        desc.Texture2DArray.ArraySize = viewDesc.arrayLayers;
                        desc.Texture2DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                        desc.Texture2DArray.PlaneSlice = 0;
                        desc.Texture2DArray.MipSlice = viewDesc.baseMipLevel;
                        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                    } else {
                        desc.Texture2D.PlaneSlice = 0;
                        desc.Texture2D.MipSlice = viewDesc.baseMipLevel;
                        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    }
                } break;

                case ITexture::Description::Type::Texture3D: {
                    assert(texDesc.arrayLayers <= 1);
                    desc.Texture3D.FirstWSlice = 0;
                    desc.Texture3D.WSize = -1;
                    desc.Texture3D.MipSlice = viewDesc.baseMipLevel;
                    desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                } break;
            }

            return desc;
        }

        common::sp<IBindableResource> LifetimeRefOf(const ResourceDescriptorWrite& write) {
            return std::visit([](const auto& d) -> common::sp<IBindableResource> {
                using D = std::decay_t<decltype(d)>;
                if constexpr(std::is_same_v<D, UniformBufferDescriptor> ||
                             std::is_same_v<D, ReadOnlyStorageBufferDescriptor> ||
                             std::is_same_v<D, ReadWriteStorageBufferDescriptor>)
                    return d.buffer;
                else
                    return d.texture;
            }, write);
        }

    } // namespace

    common::sp<IResourceDescriptorHeap> DXCResourceDescriptorHeap::Make(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    ) {
        return common::sp(new DXCResourceDescriptorHeap(dev, desc));
    }

    DXCResourceDescriptorHeap::DXCResourceDescriptorHeap(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    )
        : _dev(dev)
        , _desc(desc)
        , _heap(nullptr)
        , _boundResources(desc.capacity)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = desc.capacity;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        ThrowIfFailed(_dev->GetDevice()->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(&_heap)));

        _descriptorSize = _dev->GetDevice()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    
    DXCResourceDescriptorHeap::~DXCResourceDescriptorHeap() {
        if(_heap)
            _heap->Release();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DXCResourceDescriptorHeap::_GetCpuHandle(
        std::uint32_t index
    ) const {
        return OffsetDescriptor(
            _heap->GetCPUDescriptorHandleForHeapStart(),
            _descriptorSize,
            index);
    }

    bool DXCResourceDescriptorHeap::_RequiresTier2NullDescriptor(
        D3D12_DESCRIPTOR_RANGE_TYPE type
    ) const {
        auto tier =  _dev->GetDevCaps().options.ResourceBindingTier;
        if(tier == D3D12_RESOURCE_BINDING_TIER_1) {
            return true;
        }

        if(tier == D3D12_RESOURCE_BINDING_TIER_2) {
            return type == D3D12_DESCRIPTOR_RANGE_TYPE_CBV ||
                    type == D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }

        return false;
    }

    void DXCResourceDescriptorHeap::Write(
        ResourceDescriptorIndex index,
        const ResourceDescriptorWrite& write
    ) {
        auto* device = _dev->GetDevice();
        auto dst = _GetCpuHandle(index.value);

        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;

            if constexpr(std::is_same_v<T, UniformBufferDescriptor>) {
                if(!value.buffer) {
                    if(_RequiresTier2NullDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_CBV)) {
                        device->CreateConstantBufferView(nullptr, dst);
                    }
                    return;
                }

                auto desc = MakeCbvDesc(*value.buffer);
                device->CreateConstantBufferView(&desc, dst);
            } else if constexpr(std::is_same_v<T, ReadOnlyStorageBufferDescriptor>) {
                if(!value.buffer) {
                    if(_RequiresTier2NullDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_SRV)) {
                        device->CreateConstantBufferView(nullptr, dst);
                    }
                    return;
                }

                auto* dxcBuffer =
                    PtrCast<DXCBuffer>(value.buffer->GetBufferObject().get());
                auto srvDesc = MakeBufferSrvDesc(*value.buffer);
                device->CreateShaderResourceView(dxcBuffer->GetHandle(), &srvDesc, dst);
            } else if constexpr(std::is_same_v<T, ReadWriteStorageBufferDescriptor>) {
                if(!value.buffer) {
                    if(_RequiresTier2NullDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_UAV)) {
                        device->CreateConstantBufferView(nullptr, dst);
                    }
                    return;
                }

                auto* dxcBuffer =
                    PtrCast<DXCBuffer>(value.buffer->GetBufferObject().get());
                auto uavDesc = MakeBufferUavDesc(*value.buffer);
                device->CreateUnorderedAccessView(
                    dxcBuffer->GetHandle(),
                    nullptr,
                    &uavDesc,
                    dst);
            } else if constexpr(std::is_same_v<T, SampledTextureDescriptor>) {
                if(!value.texture) {
                    if(_RequiresTier2NullDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_SRV)) {
                        device->CreateConstantBufferView(nullptr, dst);
                    }
                    return;
                }

                auto* dxcTex =
                    PtrCast<DXCTexture>(value.texture->GetTextureObject().get());
                auto srvDesc = MakeTextureSrvDesc(*value.texture);
                device->CreateShaderResourceView(dxcTex->GetHandle(), &srvDesc, dst);
            } else if constexpr(std::is_same_v<T, StorageTextureDescriptor>) {
                if(!value.texture) {
                    if(_RequiresTier2NullDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_UAV)) {
                        device->CreateConstantBufferView(nullptr, dst);
                    }
                    return;
                }

                auto* dxcTex =
                    PtrCast<DXCTexture>(value.texture->GetTextureObject().get());
                auto uavDesc = MakeTextureUavDesc(*value.texture);
                device->CreateUnorderedAccessView(
                    dxcTex->GetHandle(),
                    nullptr,
                    &uavDesc,
                    dst);
            }
        }, write);

        _boundResources[index.value] = LifetimeRefOf(write);
    }

    void DXCResourceDescriptorHeap::WriteRange(
        ResourceDescriptorIndex firstIndex,
        std::span<const ResourceDescriptorWrite> writes
    ) {
        for(std::uint32_t i = 0; i < writes.size(); ++i) {
            Write(firstIndex + i, writes[i]);
        }
    }

    void DXCResourceDescriptorHeap::Clear(ResourceDescriptorIndex index) {
        auto tier =  _dev->GetDevCaps().options.ResourceBindingTier;
        if(tier < D3D12_RESOURCE_BINDING_TIER_3) {
            _dev->GetDevice()->CreateConstantBufferView(nullptr, _GetCpuHandle(index.value));
        }
        _boundResources[index.value] = {};
    }

    void DXCResourceDescriptorHeap::ClearRange(
        ResourceDescriptorIndex firstIndex,
        std::uint32_t count
    ) {
        for(std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }

    common::sp<ISamplerDescriptorHeap> DXCSamplerDescriptorHeap::Make (
        const common::sp<DXCDevice>& dev,
        const Description& desc
    ) {
        return common::sp(new DXCSamplerDescriptorHeap(dev, desc));
    }

    DXCSamplerDescriptorHeap::DXCSamplerDescriptorHeap(
        const common::sp<DXCDevice>& dev,
        const Description& desc
    )
        : _dev(dev)
        , _desc(desc)
        , _heap(nullptr)
        , _boundSamplers(desc.capacity)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.NumDescriptors = desc.capacity;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        ThrowIfFailed(_dev->GetDevice()->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(&_heap)));

        _descriptorSize = _dev->GetDevice()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    
    DXCSamplerDescriptorHeap::~DXCSamplerDescriptorHeap() {
        if(_heap) {
            _heap->Release();
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DXCSamplerDescriptorHeap::_GetCpuHandle (
        std::uint32_t index
    ) const {
        return OffsetDescriptor(
            _heap->GetCPUDescriptorHandleForHeapStart(),
            _descriptorSize,
            index);
    }

    void DXCSamplerDescriptorHeap::Write (
        SamplerDescriptorIndex index,
        const SamplerDescriptorWrite& sampler
    ) {
        if(!sampler) {
            Clear(index);
            return;
        }

        auto desc = MakeSamplerDesc(sampler->GetDesc());
        _dev->GetDevice()->CreateSampler(&desc, _GetCpuHandle(index.value));
        _boundSamplers[index.value] = sampler;
    }

    void DXCSamplerDescriptorHeap::WriteRange (
        SamplerDescriptorIndex firstIndex,
        std::span<const SamplerDescriptorWrite> samplers
    ) {
        for(std::uint32_t i = 0; i < samplers.size(); ++i) {
            Write(firstIndex + i, samplers[i]);
        }
    }

    void DXCSamplerDescriptorHeap::Clear(SamplerDescriptorIndex index) {
        
        auto tier =  _dev->GetDevCaps().options.ResourceBindingTier;
        if(tier == D3D12_RESOURCE_BINDING_TIER_1) {
            D3D12_SAMPLER_DESC desc{};
            _dev->GetDevice()->CreateSampler(&desc, _GetCpuHandle(index.value));
        }
        _boundSamplers[index.value] = {};
    }

    void DXCSamplerDescriptorHeap::ClearRange (
        SamplerDescriptorIndex firstIndex,
        std::uint32_t count
    ) {
        for(std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }

} // namespace alloy::dxc
