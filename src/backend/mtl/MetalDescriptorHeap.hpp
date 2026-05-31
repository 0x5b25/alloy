#pragma once

#include "alloy/DescriptorHeap.hpp"

#import <Metal/Metal.h>

#include <cstdint>
#include <vector>

namespace alloy::mtl {

    class MetalDevice;

    class MetalResourceDescriptorHeap final : public IResourceDescriptorHeap {
        common::sp<MetalDevice> _dev;
        Description _desc;
        id<MTLBuffer> _argBuf;
        std::vector<common::sp<IBindableResource>> _entries;

        MetalResourceDescriptorHeap(
            const common::sp<MetalDevice>& dev,
            const Description& desc);

    public:
        ~MetalResourceDescriptorHeap() override;

        static common::sp<IResourceDescriptorHeap> Make(
            const common::sp<MetalDevice>& dev,
            const Description& desc);

        const Description& GetDesc() const override { return _desc; }

        void Write(
            ResourceDescriptorIndex index,
            const ResourceDescriptorWrite& write) override;
        void WriteRange(
            ResourceDescriptorIndex firstIndex,
            std::span<const ResourceDescriptorWrite> writes) override;
        void Clear(ResourceDescriptorIndex index) override;
        void ClearRange(ResourceDescriptorIndex firstIndex, std::uint32_t count) override;

        id<MTLBuffer> GetHandle() const { return _argBuf; }
    };

    class MetalSamplerDescriptorHeap final : public ISamplerDescriptorHeap {
        common::sp<MetalDevice> _dev;
        Description _desc;
        id<MTLBuffer> _argBuf;
        std::vector<common::sp<ISampler>> _entries;

        MetalSamplerDescriptorHeap(
            const common::sp<MetalDevice>& dev,
            const Description& desc);

    public:
        ~MetalSamplerDescriptorHeap() override;

        static common::sp<ISamplerDescriptorHeap> Make(
            const common::sp<MetalDevice>& dev,
            const Description& desc);

        const Description& GetDesc() const override { return _desc; }

        void Write(
            SamplerDescriptorIndex index,
            const SamplerDescriptorWrite& sampler) override;
        void WriteRange(
            SamplerDescriptorIndex firstIndex,
            std::span<const SamplerDescriptorWrite> samplers) override;
        void Clear(SamplerDescriptorIndex index) override;
        void ClearRange(SamplerDescriptorIndex firstIndex, std::uint32_t count) override;

        id<MTLBuffer> GetHandle() const { return _argBuf; }
    };

}
