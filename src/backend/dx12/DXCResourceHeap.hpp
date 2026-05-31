#pragma once

#include "alloy/DescriptorHeap.hpp"

#include <d3d12.h>

#include <vector>

namespace alloy::dxc {

    class DXCDevice;

    class DXCResourceDescriptorHeap final : public IResourceDescriptorHeap {
        
        common::sp<DXCDevice> _dev;
        Description _desc;
        ID3D12DescriptorHeap* _heap;
        std::uint32_t _descriptorSize = 0;
        std::vector<common::sp<IBindableResource>> _boundResources;

        D3D12_CPU_DESCRIPTOR_HANDLE _GetCpuHandle(std::uint32_t index) const;
        bool _RequiresTier2NullDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE type) const;

    public:
        static common::sp<IResourceDescriptorHeap> Make(
            const common::sp<DXCDevice>& dev,
            const Description& desc);

        DXCResourceDescriptorHeap(
            const common::sp<DXCDevice>& dev,
            const Description& desc);

        virtual ~DXCResourceDescriptorHeap() override;

        const Description& GetDesc() const override { return _desc; }

        void Write(
            ResourceDescriptorIndex index,
            const ResourceDescriptorWrite& write) override;

        void WriteRange(
            ResourceDescriptorIndex firstIndex,
            std::span<const ResourceDescriptorWrite> writes) override;

        void Clear(ResourceDescriptorIndex index) override;
        void ClearRange(ResourceDescriptorIndex firstIndex, std::uint32_t count) override;

        ID3D12DescriptorHeap* GetHandle() const { return _heap; }
    };

    class DXCSamplerDescriptorHeap final : public ISamplerDescriptorHeap {
        
        common::sp<DXCDevice> _dev;
        Description _desc;
        ID3D12DescriptorHeap* _heap;
        std::uint32_t _descriptorSize = 0;
        std::vector<common::sp<ISampler>> _boundSamplers;

        D3D12_CPU_DESCRIPTOR_HANDLE _GetCpuHandle(std::uint32_t index) const;

    public:
        static common::sp<ISamplerDescriptorHeap> Make(
            const common::sp<DXCDevice>& dev,
            const Description& desc);

        DXCSamplerDescriptorHeap(
            const common::sp<DXCDevice>& dev,
            const Description& desc);

        virtual ~DXCSamplerDescriptorHeap() override;

        const Description& GetDesc() const override { return _desc; }

        void Write(
            SamplerDescriptorIndex index,
            const SamplerDescriptorWrite& sampler) override;

        void WriteRange(
            SamplerDescriptorIndex firstIndex,
            std::span<const SamplerDescriptorWrite> samplers) override;

        void Clear(SamplerDescriptorIndex index) override;
        void ClearRange(SamplerDescriptorIndex firstIndex, std::uint32_t count) override;

        ID3D12DescriptorHeap* GetHandle() const { return _heap; }
    };

} // namespace alloy::dxc
