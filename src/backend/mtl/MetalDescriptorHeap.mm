#include "MetalDescriptorHeap.hpp"

#include "MetalDevice.h"
#include "MetalTexture.h"

#include "alloy/common/Common.hpp"

#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

#include <cassert>
#include <type_traits>
#include <variant>

namespace alloy::mtl {

    namespace {
        IRDescriptorTableEntry* OffsetDescriptorTableEntry(
            id<MTLBuffer> argBuf,
            std::uint32_t descriptorIndex)
        {
            auto entryStartCpuAddr = (std::uintptr_t)[argBuf contents];
            return (IRDescriptorTableEntry*)(
                entryStartCpuAddr + descriptorIndex * sizeof(IRDescriptorTableEntry));
        }

        common::sp<IBindableResource> EncodeResourceDescriptor(
            IRDescriptorTableEntry* dst,
            const ResourceDescriptorWrite& write)
        {
            common::sp<IBindableResource> lifetimeRef;

            std::visit([&](const auto& d) {
                using D = std::decay_t<decltype(d)>;
                if constexpr(std::is_same_v<D, UniformBufferDescriptor> ||
                             std::is_same_v<D, ReadOnlyStorageBufferDescriptor> ||
                             std::is_same_v<D, ReadWriteStorageBufferDescriptor>) {
                    if(!d.buffer) {
                        return;
                    }

                    auto* mtlBuffer =
                        static_cast<const MetalBuffer*>(d.buffer->GetBufferObject().get());

                    auto baseGPUAddr = [mtlBuffer->GetHandle() gpuAddress];
                    auto byteCnt = d.buffer->GetShape().GetSizeInBytes();

                    assert( (!std::is_same_v<D, UniformBufferDescriptor>)
                           || (byteCnt % 256 == 0) );

                    IRDescriptorTableSetBuffer(
                        dst,
                        baseGPUAddr + d.buffer->GetShape().GetOffsetInBytes(),
                        0);

                    lifetimeRef = d.buffer;
                } else {
                    if(!d.texture) {
                        return;
                    }

                    auto* texView = common::PtrCast<ITextureView>(d.texture.get());
                    auto* mtlTex =
                        common::PtrCast<MetalTexture>(texView->GetTextureObject().get());

                    IRDescriptorTableSetTexture(dst, mtlTex->GetHandle(), 0, 0);

                    lifetimeRef = d.texture;
                }
            }, write);

            return lifetimeRef;
        }
    }

    MetalResourceDescriptorHeap::MetalResourceDescriptorHeap(
        const common::sp<MetalDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
        , _argBuf(nil)
        , _entries(desc.capacity)
    {
        @autoreleasepool {
            auto bufferSize = desc.capacity * sizeof(IRDescriptorTableEntry);
            if(bufferSize == 0) {
                return;
            }

            _argBuf = [_dev->GetHandle() newBufferWithLength:bufferSize
                                                     options:MTLResourceStorageModeShared];
            [_argBuf setLabel:@"ResourceDescriptorHeap"];
        }
    }

    MetalResourceDescriptorHeap::~MetalResourceDescriptorHeap() {
        @autoreleasepool {
            [_argBuf release];
        }
    }

    common::sp<IResourceDescriptorHeap> MetalResourceDescriptorHeap::Make(
        const common::sp<MetalDevice>& dev,
        const Description& desc)
    {
        return common::sp<IResourceDescriptorHeap>(
            new MetalResourceDescriptorHeap(dev, desc));
    }

    void MetalResourceDescriptorHeap::Write(
        ResourceDescriptorIndex index,
        const ResourceDescriptorWrite& write)
    {
        assert(index.value < _desc.capacity);
        auto* dst = OffsetDescriptorTableEntry(_argBuf, index.value);
        _entries[index.value] = EncodeResourceDescriptor(dst, write);
    }

    void MetalResourceDescriptorHeap::WriteRange(
        ResourceDescriptorIndex firstIndex,
        std::span<const ResourceDescriptorWrite> writes)
    {
        assert(firstIndex.value + writes.size() <= _desc.capacity);
        for(std::uint32_t i = 0; i < writes.size(); ++i) {
            Write(firstIndex + i, writes[i]);
        }
    }

    void MetalResourceDescriptorHeap::Clear(ResourceDescriptorIndex index) {
        assert(index.value < _desc.capacity);
        _entries[index.value].reset();
    }

    void MetalResourceDescriptorHeap::ClearRange(
        ResourceDescriptorIndex firstIndex,
        std::uint32_t count)
    {
        assert(firstIndex.value + count <= _desc.capacity);
        for(std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }

    MetalSamplerDescriptorHeap::MetalSamplerDescriptorHeap(
        const common::sp<MetalDevice>& dev,
        const Description& desc)
        : _dev(dev)
        , _desc(desc)
        , _argBuf(nil)
        , _entries(desc.capacity)
    {
        @autoreleasepool {
            auto bufferSize = desc.capacity * sizeof(IRDescriptorTableEntry);
            if(bufferSize == 0) {
                return;
            }

            _argBuf = [_dev->GetHandle() newBufferWithLength:bufferSize
                                                     options:MTLResourceStorageModeShared];
            [_argBuf setLabel:@"SamplerDescriptorHeap"];
        }
    }

    MetalSamplerDescriptorHeap::~MetalSamplerDescriptorHeap() {
        @autoreleasepool {
            [_argBuf release];
        }
    }

    common::sp<ISamplerDescriptorHeap> MetalSamplerDescriptorHeap::Make(
        const common::sp<MetalDevice>& dev,
        const Description& desc)
    {
        return common::sp<ISamplerDescriptorHeap>(
            new MetalSamplerDescriptorHeap(dev, desc));
    }

    void MetalSamplerDescriptorHeap::Write(
        SamplerDescriptorIndex index,
        const SamplerDescriptorWrite& sampler)
    {
        assert(index.value < _desc.capacity);
        if(!sampler) {
            _entries[index.value].reset();
            return;
        }

        auto* mtlSampler = common::PtrCast<MetalSampler>(sampler.get());
        auto* dst = OffsetDescriptorTableEntry(_argBuf, index.value);
        IRDescriptorTableSetSampler(dst, mtlSampler->GetHandle(), 0);

        _entries[index.value] = sampler;
    }

    void MetalSamplerDescriptorHeap::WriteRange(
        SamplerDescriptorIndex firstIndex,
        std::span<const SamplerDescriptorWrite> samplers)
    {
        assert(firstIndex.value + samplers.size() <= _desc.capacity);
        for(std::uint32_t i = 0; i < samplers.size(); ++i) {
            Write(firstIndex + i, samplers[i]);
        }
    }

    void MetalSamplerDescriptorHeap::Clear(SamplerDescriptorIndex index) {
        assert(index.value < _desc.capacity);
        _entries[index.value].reset();
    }

    void MetalSamplerDescriptorHeap::ClearRange(
        SamplerDescriptorIndex firstIndex,
        std::uint32_t count)
    {
        assert(firstIndex.value + count <= _desc.capacity);
        for(std::uint32_t i = 0; i < count; ++i) {
            Clear(firstIndex + i);
        }
    }

}
